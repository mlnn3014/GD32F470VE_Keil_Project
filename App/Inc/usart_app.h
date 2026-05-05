#ifndef USART_APP_H
#define USART_APP_H

#include <stdint.h>

#include "usart_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*uart_line_handler_t)(const char *line);

int uart_printf(uint32_t uart, const char *format, ...);
void uart_app_init(void);
void uart_on_line(uart_line_handler_t handler);
void uart_task(void);

#ifdef __cplusplus
}
#endif /* USART_APP_H */

#endif
