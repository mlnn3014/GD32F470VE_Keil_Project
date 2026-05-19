#include "oled_app.h"

#include "adc_app.h"
#include "gd30_app.h"
#include "oled.h"
#include "rtc_app.h"
#include "systick.h"

#define OLED_ROW_TEMP 0U
#define OLED_ROW_ADC  1U
#define OLED_ROW_REF  2U
#define OLED_ROW_TIME 3U

static void oled_draw_status(void)
{
    adc_data_t adc_data = adc_get_data();
    gd30_pt100_data_t pt100 = gd30_get_pt100();
    rtc_time_t rtc_time = rtc_get_time();
    int32_t temp = pt100.temperature_centi_c;
    int32_t temp_abs;
    char temp_sign = '+';

    if (temp < 0) {
        temp_sign = '-';
        temp_abs = -temp;
    } else {
        temp_abs = temp;
    }

    if (pt100.valid != 0U) {
        (void)oled_text_printf(OLED_FONT_8, OLED_ROW_TEMP, 0U, 0U,
                               "PT100:%c%ld.%02ldC",
                               temp_sign,
                               temp_abs / 100L,
                               temp_abs % 100L);
        (void)oled_text_printf(OLED_FONT_8, OLED_ROW_ADC, 0U, 0U,
                               "RAW:%6d %4ldmV",
                               pt100.raw_sample,
                               pt100.adc_microvolt / 1000L);
    } else {
        (void)oled_text_show(OLED_FONT_8, OLED_ROW_TEMP, 0U, 0U, "PT100: waiting");
        (void)oled_text_printf(OLED_FONT_8, OLED_ROW_ADC, 0U, 0U,
                               "ADC:%4u %4umV",
                               adc_data.sample,
                               adc_data.millivolt);
    }

    if (pt100.reference_valid != 0U) {
        (void)oled_text_printf(OLED_FONT_8, OLED_ROW_REF, 0U, 0U,
                               "EXTREF:%4ldmV %s",
                               pt100.reference_microvolt / 1000L,
                               (pt100.external_reference_enabled != 0U) ? "ON" : "OFF");
    } else {
        (void)oled_text_show(OLED_FONT_8, OLED_ROW_REF, 0U, 0U, "EXTREF: waiting");
    }

    (void)oled_text_printf(OLED_FONT_8, OLED_ROW_TIME, 0U, 0U,
                           "TIME:%02u:%02u:%02u",
                           rtc_time.hour,
                           rtc_time.minute,
                           rtc_time.second);
}

void oled_task(void)
{
    oled_draw_status();
    (void)oled_service();
}
