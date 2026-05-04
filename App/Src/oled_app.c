#include "mcu_cmic_gd32f470vet6.h"

static void oled_app_draw_tick(void)
{
    (void)oled_text_printf(OLED_FONT_8, 0U, 0U, 0U, "uwTick: %u", systick_get_ms());
}

static void oled_app_draw_status(void)
{
    rtc_time_t time;

    rtc_app_get_time(&time);

    (void)oled_text_printf(OLED_FONT_8, 2U, 0U, 0U,
                           "ADC: %0.2x:%0.2x:%0.2x", time.hour, time.minute, time.second);
}

void oled_task(void)
{
    oled_app_draw_tick();
    oled_app_draw_status();
    (void)oled_service();
}
