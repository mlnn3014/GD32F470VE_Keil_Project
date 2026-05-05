#include "oled_app.h"

#include "adc_app.h"
#include "oled.h"
#include "rtc_app.h"
#include "systick.h"

#define OLED_ROW_TICK 0U
#define OLED_ROW_ADC  2U
#define OLED_ROW_TIME 3U

static void oled_draw_tick(void)
{
    (void)oled_text_printf(OLED_FONT_8, OLED_ROW_TICK, 0U, 0U, "uwTick:%u", systick_get_ms());
}

static void oled_draw_status(void)
{
    adc_data_t adc_data = adc_get_data();
    rtc_time_t rtc_time = rtc_get_time();

    (void)oled_text_printf(OLED_FONT_8, OLED_ROW_ADC, 0U, 0U,
                           "ADC:%4u %4umV",
                           adc_data.sample,
                           adc_data.millivolt);
    (void)oled_text_printf(OLED_FONT_8, OLED_ROW_TIME, 0U, 0U,
                           "TIME:%02u:%02u:%02u",
                           rtc_time.hour,
                           rtc_time.minute,
                           rtc_time.second);
}

void oled_task(void)
{
    oled_draw_tick();
    oled_draw_status();
    (void)oled_service();
}
