#include "gd30_app.h"

#include "gd30_bsp.h"
#include "systick.h"
#include "usart_app.h"

#define GD30_PT100_CHANNEL        GD30_CH0
#define GD30_PT100_PGA            GD30_PGA_2V048
#define GD30_SAMPLE_RATE          GD30_RATE_12_5SPS
#define GD30_TELEMETRY_PERIOD_MS  1000U
#define GD30_PROCESS_REGISTER     0x0012U
#define GD30_PROCESS_VALUE        0xACCAU
#define GD30_EXTREF_REGISTER      0x0014U
#define GD30_EXTREF_ENABLE_BIT    0x0040U
#define GD30_EXTREF_READ_CMD      0x8106U
#define GD30_EXTREF_WRITE_CMD     0x8100U
#define GD30_REFERENCE_UV         2500000

#define PT100_GAIN                16
#define PT100_OFFSET_UV           409600
#define PT100_R_MIN_MILLIOHM      80310
#define PT100_ADC_MIN_UV          409600
#define PT100_ADC_MAX_UV          1641920

typedef struct {
    int32_t temp_centi_c;
    int32_t resistance_milliohm;
} pt100_point_t;

typedef struct {
    gd30_channel_t channel;
    gd30_pga_t pga;
} gd30_sequence_step_t;

static const gd30_sequence_step_t gd30_sequence[] = {
    {GD30_PT100_CHANNEL, GD30_PT100_PGA}
};

static const pt100_point_t pt100_lut[] = {
    {-5000, 80310},
    {-4926, 80600},
    {-4447, 82500},
    {0,     100000},
    {2002,  107800},
    {3343,  113000},
    {3860,  115000},
    {3989,  115500},
    {5989,  123200},
    {8001,  130900},
    {9999,  138500},
    {13045, 150000},
    {14111, 154000},
    {15000, 157330}
};

static volatile gd30_data_t gd30_data;
static volatile gd30_pt100_data_t gd30_pt100;
static uint32_t gd30_active_step;
static uint32_t gd30_next_step;
static uint32_t gd30_next_read_ms;
static uint32_t gd30_wait_ms;
static uint32_t gd30_next_telemetry_ms;

static void gd30_write_extended_register(uint16_t address, uint16_t value)
{
    const uint16_t cmd[] = {
        GD30_EXTREF_WRITE_CMD,
        address,
        value
    };

    gd30_transfer16_sequence(cmd, 0, 3U);
    delay_1ms(1U);
}

static uint16_t gd30_read_extended_register(uint16_t address)
{
    const uint16_t cmd[] = {
        GD30_EXTREF_READ_CMD,
        address,
        0x0000U
    };
    uint16_t rx[3] = {0U, 0U, 0U};

    gd30_transfer16_sequence(cmd, rx, 3U);
    delay_1ms(1U);

    return rx[2];
}

static void gd30_enable_external_reference(void)
{
    uint16_t value;

    gd30_write_extended_register(GD30_PROCESS_REGISTER, GD30_PROCESS_VALUE);
    value = gd30_read_extended_register(GD30_EXTREF_REGISTER);
    gd30_write_extended_register(GD30_EXTREF_REGISTER, (uint16_t)(value | GD30_EXTREF_ENABLE_BIT));

    gd30_pt100.reference_microvolt = GD30_REFERENCE_UV;
    gd30_pt100.reference_valid = 1U;
    gd30_pt100.external_reference_enabled = 1U;
}

static int32_t pt100_adc_to_resistance_milliohm(int32_t adc_microvolt)
{
    int64_t value;

    value = (int64_t)(adc_microvolt - PT100_OFFSET_UV);
    value /= PT100_GAIN;
    value += PT100_R_MIN_MILLIOHM;

    if (value < 0) {
        value = 0;
    }

    return (int32_t)value;
}

static int32_t pt100_resistance_to_centi_c(int32_t resistance_milliohm)
{
    uint32_t i;

    if (resistance_milliohm <= pt100_lut[0].resistance_milliohm) {
        return pt100_lut[0].temp_centi_c;
    }

    for (i = 1U; i < (sizeof(pt100_lut) / sizeof(pt100_lut[0])); i++) {
        const pt100_point_t *low = &pt100_lut[i - 1U];
        const pt100_point_t *high = &pt100_lut[i];

        if (resistance_milliohm <= high->resistance_milliohm) {
            int64_t num = (int64_t)(resistance_milliohm - low->resistance_milliohm);
            int32_t den = high->resistance_milliohm - low->resistance_milliohm;
            int32_t span = high->temp_centi_c - low->temp_centi_c;

            if (den == 0) {
                return low->temp_centi_c;
            }

            num *= span;
            num /= den;
            return (int32_t)(low->temp_centi_c + num);
        }
    }

    return pt100_lut[(sizeof(pt100_lut) / sizeof(pt100_lut[0])) - 1U].temp_centi_c;
}

static int32_t pt100_adc_to_centi_c(int32_t adc_microvolt)
{
    int32_t resistance_milliohm;

    if (adc_microvolt < PT100_ADC_MIN_UV) {
        adc_microvolt = PT100_ADC_MIN_UV;
    } else if (adc_microvolt > PT100_ADC_MAX_UV) {
        adc_microvolt = PT100_ADC_MAX_UV;
    }

    resistance_milliohm = pt100_adc_to_resistance_milliohm(adc_microvolt);
    return pt100_resistance_to_centi_c(resistance_milliohm);
}

static void gd30_save_sample(gd30_channel_t channel, gd30_pga_t pga, int16_t sample)
{
    int32_t microvolt;

    microvolt = gd30_sample_to_microvolt(sample, pga);
    gd30_data.channel[channel].sample = sample;
    gd30_data.channel[channel].microvolt = microvolt;
    gd30_data.channel[channel].pga = pga;
    gd30_data.channel[channel].valid = 1U;

    if (channel == GD30_PT100_CHANNEL) {
        gd30_pt100.raw_sample = sample;
        gd30_pt100.adc_microvolt = microvolt;
        gd30_pt100.temperature_centi_c = pt100_adc_to_centi_c(microvolt);
        gd30_pt100.valid = 1U;
    }
}

static uint16_t gd30_step_config(uint32_t step)
{
    const gd30_sequence_step_t *seq;

    if (step >= (sizeof(gd30_sequence) / sizeof(gd30_sequence[0]))) {
        step = 0U;
    }

    seq = &gd30_sequence[step];
    return gd30_make_config(seq->channel, seq->pga, GD30_SAMPLE_RATE);
}

static void gd30_report(void)
{
    gd30_pt100_data_t pt100 = gd30_get_pt100();
    int32_t temp = pt100.temperature_centi_c;
    int32_t temp_abs;
    char temp_sign = '+';

    if (pt100.valid == 0U) {
        return;
    }

    if (temp < 0) {
        temp_sign = '-';
        temp_abs = -temp;
    } else {
        temp_abs = temp;
    }

    uart_printf(DEBUG_USART,
                "PT100: raw=%d adc=%lduV temp=%c%ld.%02ldC extref=%lduV %s\r\n",
                pt100.raw_sample,
                pt100.adc_microvolt,
                temp_sign,
                temp_abs / 100L,
                temp_abs % 100L,
                pt100.reference_microvolt,
                (pt100.external_reference_enabled != 0U) ? "ON" : "OFF");
}

void gd30_app_init(void)
{
    gd30_enable_external_reference();

    gd30_active_step = 0U;
    gd30_next_step = 0U;
    gd30_wait_ms = gd30_rate_wait_ms(GD30_SAMPLE_RATE);

    (void)gd30_transfer16(gd30_step_config(gd30_active_step));
    gd30_next_read_ms = systick_get_ms() + gd30_wait_ms;
    gd30_next_telemetry_ms = systick_get_ms() + GD30_TELEMETRY_PERIOD_MS;
}

void gd30_task(void)
{
    uint32_t now = systick_get_ms();
    const gd30_sequence_step_t *active;
    int16_t sample;

    if ((int32_t)(now - gd30_next_read_ms) < 0) {
        return;
    }

    active = &gd30_sequence[gd30_active_step];
    sample = (int16_t)gd30_transfer16(gd30_step_config(gd30_next_step));
    gd30_save_sample(active->channel, active->pga, sample);

    gd30_active_step = gd30_next_step;
    gd30_next_step++;
    if (gd30_next_step >= (sizeof(gd30_sequence) / sizeof(gd30_sequence[0]))) {
        gd30_next_step = 0U;
    }
    gd30_next_read_ms = now + gd30_wait_ms;

    if ((int32_t)(now - gd30_next_telemetry_ms) >= 0) {
        gd30_next_telemetry_ms = now + GD30_TELEMETRY_PERIOD_MS;
        gd30_report();
    }
}

gd30_data_t gd30_get_data(void)
{
    gd30_data_t data;
    uint32_t i;

    for (i = 0U; i < GD30_CHANNEL_COUNT; i++) {
        data.channel[i].sample = gd30_data.channel[i].sample;
        data.channel[i].microvolt = gd30_data.channel[i].microvolt;
        data.channel[i].pga = gd30_data.channel[i].pga;
        data.channel[i].valid = gd30_data.channel[i].valid;
    }

    return data;
}

gd30_channel_data_t gd30_get_channel(gd30_channel_t channel)
{
    gd30_channel_data_t data = {0};

    if ((uint32_t)channel < GD30_CHANNEL_COUNT) {
        data.sample = gd30_data.channel[channel].sample;
        data.microvolt = gd30_data.channel[channel].microvolt;
        data.pga = gd30_data.channel[channel].pga;
        data.valid = gd30_data.channel[channel].valid;
    }

    return data;
}

gd30_pt100_data_t gd30_get_pt100(void)
{
    gd30_pt100_data_t data;

    data.raw_sample = gd30_pt100.raw_sample;
    data.adc_microvolt = gd30_pt100.adc_microvolt;
    data.temperature_centi_c = gd30_pt100.temperature_centi_c;
    data.reference_microvolt = gd30_pt100.reference_microvolt;
    data.valid = gd30_pt100.valid;
    data.reference_valid = gd30_pt100.reference_valid;
    data.external_reference_enabled = gd30_pt100.external_reference_enabled;

    return data;
}
