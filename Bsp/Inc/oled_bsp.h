#ifndef OLED_BSP_H
#define OLED_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t oled_bsp_iic_init(void);
uint8_t oled_bsp_iic_deinit(void);
uint8_t oled_bsp_iic_write(uint8_t addr, uint8_t control, uint8_t *buf, uint16_t len);
void oled_bsp_delay_ms(uint32_t ms);
void oled_bsp_debug_print(const char *const fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* OLED_BSP_H */
