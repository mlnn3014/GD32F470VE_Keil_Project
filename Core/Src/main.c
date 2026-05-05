#include "main.h"

int main(void)
{
    int rtc_init_result;
    int flash_init_result;
    flash_info_t flash_info;

    systick_config();

    led_app_init();
    uart_init();
    uart_app_init();

    uart_printf(DEBUG_USART, "BOOT: start\r\n");

    uart_printf(DEBUG_USART, "BOOT: flash init...\r\n");
    flash_init_result = flash_init();
    flash_info = flash_get_info();
    uart_printf(DEBUG_USART,
                "BOOT: flash done (%d, id=0x%06lX, size=%lu)\r\n",
                flash_init_result,
                flash_info.id,
                flash_info.size);

    uart_printf(DEBUG_USART, "BOOT: gd30 init...\r\n");
    gd30_bus_init();
    gd30_app_init();
    uart_printf(DEBUG_USART, "BOOT: gd30 done\r\n");

    uart_printf(DEBUG_USART, "BOOT: adc init...\r\n");
    adc_init();
    adc_app_init();
    uart_printf(DEBUG_USART, "BOOT: adc done\r\n");

    uart_printf(DEBUG_USART, "BOOT: dac init...\r\n");
    dac_init();
    dac_app_init();
    uart_printf(DEBUG_USART, "BOOT: dac done\r\n");

    uart_printf(DEBUG_USART, "BOOT: rtc init...\r\n");
    rtc_init_result = rtc_clock_init();
    rtc_app_init();
    uart_printf(DEBUG_USART, "BOOT: rtc done (%d, %s)\r\n", rtc_init_result, rtc_source_name());

    sd_fatfs_init();
    btn_app_init();

    uart_printf(DEBUG_USART, "BOOT: oled init...\r\n");
    if (oled_init() == 0U) {
        uart_printf(DEBUG_USART, "BOOT: oled done\r\n");
        oled_clear();
        oled_text_show(OLED_FONT_8, 0U, 0U, 0U, "BOOT: start");
        (void)oled_update();
    } else {
        uart_printf(DEBUG_USART, "BOOT: oled failed\r\n");
    }

#if FLASH_SELF_TEST_ENABLE
    (void)flash_self_test(0U);
#else
    uart_printf(DEBUG_USART, "BOOT: flash self test skipped (FLASH_SELF_TEST_ENABLE=0)\r\n");
#endif
#if SD_FATFS_DEMO_ENABLE
    sd_fatfs_test();
#else
    uart_printf(DEBUG_USART, "BOOT: sd_fatfs_test skipped (SD_FATFS_DEMO_ENABLE=0)\r\n");
#endif

    scheduler_init();
    while(1) {
        scheduler_run();
    }
}

#ifdef GD_ECLIPSE_GCC
/* retarget the C library printf function to the USART, in Eclipse GCC environment */
int __io_putchar(int ch)
{
    uint8_t data = (uint8_t)ch;

    (void)uart_write(&data, 1U);
    return ch;
}
#else
/* retarget the C library printf function to the USART */
int fputc(int ch, FILE *f)
{
    uint8_t data = (uint8_t)ch;

    (void)f;
    (void)uart_write(&data, 1U);
    return ch;
}
#endif /* GD_ECLIPSE_GCC */
