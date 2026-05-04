#include "mcu_cmic_gd32f470vet6.h"

static rtc_time_t rtc_now;

void rtc_task(void)
{
    rtc_parameter_struct time;

    rtc_current_time_get(&time);
    rtc_now.hour = time.hour;
    rtc_now.minute = time.minute;
    rtc_now.second = time.second;
}

void rtc_app_get_time(rtc_time_t *time)
{
    if (time == NULL) {
        return;
    }

    *time = rtc_now;
}
