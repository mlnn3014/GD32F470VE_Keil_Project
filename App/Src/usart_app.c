#include "usart_app.h"

#include <stdarg.h>
#include <stdio.h>

#define USART_APP_PRINTF_BUFFER_SIZE 512U
#define USART_APP_LINE_BUFFER_SIZE   128U
#define USART_APP_READ_BUFFER_SIZE   64U

static char uart_line[USART_APP_LINE_BUFFER_SIZE];
static uint16_t uart_line_len;
static uart_line_handler_t uart_line_handler;

static void uart_default_line_handler(const char *line)
{
    uart_printf(DEBUG_USART, "CMD: %s\r\n", line);
}

static void uart_dispatch_line(void)
{
    if (uart_line_len == 0U) {
        return;
    }

    uart_line[uart_line_len] = '\0';

    if (uart_line_handler != 0) {
        uart_line_handler(uart_line);
    } else {
        uart_default_line_handler(uart_line);
    }

    uart_line_len = 0U;
}

static void uart_receive_byte(uint8_t data)
{
    if ((data == '\r') || (data == '\n')) {
        uart_dispatch_line();
        return;
    }

    if (uart_line_len < (USART_APP_LINE_BUFFER_SIZE - 1U)) {
        uart_line[uart_line_len] = (char)data;
        uart_line_len++;
    } else {
        uart_line_len = 0U;
        uart_printf(DEBUG_USART, "CMD too long\r\n");
    }
}

int uart_printf(uint32_t uart, const char *format, ...)
{
    char buffer[USART_APP_PRINTF_BUFFER_SIZE];
    va_list arg;
    int len;

    (void)uart;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (len <= 0) {
        return len;
    }

    if ((uint32_t)len >= sizeof(buffer)) {
        len = (int)(sizeof(buffer) - 1U);
    }

    return (int)uart_write((const uint8_t *)buffer, (uint16_t)len);
}

void uart_app_init(void)
{
    uart_line_len = 0U;
    uart_line_handler = 0;
}

void uart_on_line(uart_line_handler_t handler)
{
    uart_line_handler = handler;
}

void uart_task(void)
{
    uint8_t buf[USART_APP_READ_BUFFER_SIZE];
    uint16_t read_count;
    uint16_t i;

    uart_poll();
    do {
        read_count = uart_read(buf, USART_APP_READ_BUFFER_SIZE);
        for (i = 0U; i < read_count; i++) {
            uart_receive_byte(buf[i]);
        }
    } while (read_count == USART_APP_READ_BUFFER_SIZE);
}
