#ifndef RTC_APP_H
#define RTC_APP_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_time_t;

void rtc_task(void);
void rtc_app_get_time(rtc_time_t *time);

#ifdef __cplusplus
}
#endif

#endif /* RTC_APP_H */
