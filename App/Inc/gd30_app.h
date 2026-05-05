#ifndef GD30_APP_H
#define GD30_APP_H

#include "gd30ad3344.h"

#ifdef __cplusplus
extern "C" {
#endif

void gd30_app_init(void);
void gd30_task(void);
gd30_data_t gd30_get_data(void);
gd30_channel_data_t gd30_get_channel(gd30_channel_t channel);

#ifdef __cplusplus
}
#endif

#endif /* GD30_APP_H */
