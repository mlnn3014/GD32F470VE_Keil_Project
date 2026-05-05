#ifndef FLASH_BSP_H
#define FLASH_BSP_H

#include <stdint.h>

#include "gd32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

void flash_bus_init(void);
void flash_bus_select(void);
void flash_bus_deselect(void);
uint8_t flash_bus_transfer(uint8_t data);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_BSP_H */
