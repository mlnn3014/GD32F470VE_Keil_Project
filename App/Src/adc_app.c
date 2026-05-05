#include "adc_app.h"

#include "adc_bsp.h"

#define ADC_APP_REFERENCE_MILLIVOLT 3300U
#define ADC_APP_FULL_SCALE_VALUE    4095U

static volatile adc_data_t adc_data;

static uint16_t adc_to_millivolt(uint16_t sample)
{
    return (uint16_t)(((uint32_t)sample * ADC_APP_REFERENCE_MILLIVOLT) /
                      ADC_APP_FULL_SCALE_VALUE);
}

void adc_app_init(void)
{
    adc_task();
}

void adc_task(void)
{
    adc_data_t data;

    data.sample = adc_read();
    data.millivolt = adc_to_millivolt(data.sample);

    adc_data = data;
}

adc_data_t adc_get_data(void)
{
    adc_data_t data;

    data.sample = adc_data.sample;
    data.millivolt = adc_data.millivolt;

    return data;
}
