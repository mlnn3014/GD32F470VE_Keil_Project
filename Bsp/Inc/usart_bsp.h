#ifndef USART_BSP_H
#define USART_BSP_H

#include <stdint.h>

#include "gd32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_USART USART0

typedef struct {
    uint32_t rx_overflow_count;
    uint32_t tx_overflow_count;
    uint16_t rx_available;
    uint16_t tx_pending;
} uart_status_t;

void uart_init(void);
uint16_t uart_write(const uint8_t *data, uint16_t length);
uint16_t uart_read(uint8_t *data, uint16_t length);
uint8_t uart_read_byte(uint8_t *data);
uint16_t uart_available(void);
uart_status_t uart_status(void);
void uart_poll(void);
void uart_irq_handler(void);
void uart_tx_dma_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* USART_BSP_H */
