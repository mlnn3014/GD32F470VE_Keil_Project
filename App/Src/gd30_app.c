#include "gd30_app.h"

#include "gd30_bsp.h"
#include "systick.h"

#define GD30_DEFAULT_PGA  GD30_PGA_4V096
#define GD30_DEFAULT_RATE GD30_RATE_1000SPS

static volatile gd30_data_t gd30_data;
static gd30_channel_t gd30_active_channel;
static gd30_channel_t gd30_next_channel;
static uint32_t gd30_next_read_ms;
static uint32_t gd30_wait_ms;

static gd30_channel_t gd30_channel_next(gd30_channel_t channel)
{
    uint32_t next = (uint32_t)channel + 1U;

    if (next >= GD30_CHANNEL_COUNT) {
        next = 0U;
    }

    return (gd30_channel_t)next;
}

static void gd30_save_channel(gd30_channel_t channel, int16_t sample)
{
    gd30_data.channel[channel].sample = sample;
    gd30_data.channel[channel].microvolt = gd30_sample_to_microvolt(sample, GD30_DEFAULT_PGA);
    gd30_data.channel[channel].valid = 1U;
}

void gd30_app_init(void)
{
    uint16_t config;

    gd30_active_channel = GD30_CH0;
    gd30_next_channel = GD30_CH1;
    gd30_wait_ms = gd30_rate_wait_ms(GD30_DEFAULT_RATE);

    config = gd30_make_config(gd30_active_channel, GD30_DEFAULT_PGA, GD30_DEFAULT_RATE);
    (void)gd30_transfer16(config);
    gd30_next_read_ms = systick_get_ms() + gd30_wait_ms;
}

void gd30_task(void)
{
    uint32_t now = systick_get_ms();
    uint16_t config;
    int16_t sample;

    if ((int32_t)(now - gd30_next_read_ms) < 0) {
        return;
    }

    config = gd30_make_config(gd30_next_channel, GD30_DEFAULT_PGA, GD30_DEFAULT_RATE);
    sample = (int16_t)gd30_transfer16(config);
    gd30_save_channel(gd30_active_channel, sample);

    gd30_active_channel = gd30_next_channel;
    gd30_next_channel = gd30_channel_next(gd30_next_channel);
    gd30_next_read_ms = now + gd30_wait_ms;
}

gd30_data_t gd30_get_data(void)
{
    gd30_data_t data;
    uint32_t i;

    for (i = 0U; i < GD30_CHANNEL_COUNT; i++) {
        data.channel[i].sample = gd30_data.channel[i].sample;
        data.channel[i].microvolt = gd30_data.channel[i].microvolt;
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
        data.valid = gd30_data.channel[channel].valid;
    }

    return data;
}
