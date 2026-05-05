#ifndef GD30_BSP_H
#define GD30_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void gd30_bus_init(void);
uint16_t gd30_transfer16(uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* GD30_BSP_H */
