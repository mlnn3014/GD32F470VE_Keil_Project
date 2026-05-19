#ifndef GD30_BSP_H
#define GD30_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void gd30_bus_init(void);
uint16_t gd30_transfer16(uint16_t value);
void gd30_transfer16_sequence(const uint16_t *tx, uint16_t *rx, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* GD30_BSP_H */
