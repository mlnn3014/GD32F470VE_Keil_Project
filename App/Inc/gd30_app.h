#ifndef GD30_APP_H
#define GD30_APP_H

#include "gd30ad3344.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t raw_sample;
    int32_t adc_microvolt;
    int32_t temperature_centi_c;
    int32_t reference_microvolt;
    uint8_t valid;
    uint8_t reference_valid;
    uint8_t external_reference_enabled;
} gd30_pt100_data_t;

void gd30_app_init(void);
void gd30_task(void);
gd30_data_t gd30_get_data(void);
gd30_channel_data_t gd30_get_channel(gd30_channel_t channel);
gd30_pt100_data_t gd30_get_pt100(void);

#ifdef __cplusplus
}
#endif

#endif /* GD30_APP_H */
