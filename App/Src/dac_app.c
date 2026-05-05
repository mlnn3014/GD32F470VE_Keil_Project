#include "dac_app.h"

#include "adc_app.h"
#include "dac_bsp.h"

static volatile uint16_t dac_value;

void dac_app_init(void)
{
    dac_task();
}

void dac_task(void)
{
    adc_data_t adc_data = adc_get_data();

    dac_value = adc_data.sample;
    dac_write(dac_value);
}

uint16_t dac_get_value(void)
{
    return dac_value;
}
