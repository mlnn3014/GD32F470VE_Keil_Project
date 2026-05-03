#include "mcu_cmic_gd32f470vet6.h"

void oled_task(void)
{
    (void)oled_fill_rect(0, 0, 127, 7, 0U);
    (void)oled_fill_rect(0, 16, 47, 23, 0U);
    oled_printf(0, 0, "uwTick: %u", systick_get_ms());
    oled_printf(0, 16, "ADC:");
    (void)oled_update_async();
}
