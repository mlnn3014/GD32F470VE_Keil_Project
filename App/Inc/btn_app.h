#ifndef BTN_APP_H
#define BTN_APP_H

#include <stdint.h>
#include "btn_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

void btn_app_init(void);
void btn_task(void);

#ifdef __cplusplus
}
#endif

#endif /* BTN_APP_H */
