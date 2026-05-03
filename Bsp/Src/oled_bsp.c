#include "oled_bsp.h"

#include "driver_ssd1306.h"
#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_i2c.h"
#include "oledfont.h"
#include "systick.h"
#include "usart_app.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define OLED_I2C_PERIPH       I2C0
#define OLED_I2C_CLOCK        RCU_I2C0
#define OLED_I2C_OWN_ADDRESS  0x72U
#define OLED_I2C_WRITE_ADDR   0x78U
#define OLED_I2C_DATA_ADDRESS ((uint32_t)&I2C_DATA(OLED_I2C_PERIPH))

#define OLED_GPIO_CLOCK       RCU_GPIOB
#define OLED_GPIO_PORT        GPIOB
#define OLED_SCL_PIN          GPIO_PIN_8
#define OLED_SDA_PIN          GPIO_PIN_9

#define OLED_DMA_PERIPH       DMA0
#define OLED_DMA_CLOCK        RCU_DMA0
#define OLED_DMA_CH           DMA_CH6
#define OLED_DMA_SUBPERI      DMA_SUBPERI1
#define OLED_DMA_IRQn         DMA0_Channel6_IRQn

#define OLED_PAGE_COUNT       (OLED_HEIGHT / 8U)
#define OLED_ALL_PAGES        ((uint8_t)((1U << OLED_PAGE_COUNT) - 1U))
#define OLED_I2C_WAIT_TIMEOUT 100000U
#define OLED_TX_BUF_SIZE      (1U + (OLED_WIDTH * OLED_PAGE_COUNT))
#define OLED_CMD_BUF_SIZE     7U
#define OLED_FONT_8           8U
#define OLED_FONT_16          16U
#define OLED_ASYNC_TIMEOUT_MS 200U
#define OLED_SYNC_TIMEOUT_MS  200U

typedef enum {
    OLED_TX_IDLE = 0,
    OLED_TX_CMD,
    OLED_TX_DATA,
} oled_tx_phase_t;

static ssd1306_handle_t s_oled_handle;
static uint8_t s_oled_inited;
static uint8_t s_oled_tx_buf[OLED_TX_BUF_SIZE];
static uint8_t s_oled_cmd_buf[OLED_CMD_BUF_SIZE];
static volatile uint8_t s_dirty_pages;
static volatile uint8_t s_active_pages;
static volatile uint8_t s_oled_busy;
static volatile uint8_t s_oled_error;
static volatile oled_tx_phase_t s_tx_phase;
static volatile uint8_t s_tx_dma_done;
static volatile uint8_t s_tx_dma_error;
static uint8_t s_tx_start_page;
static uint8_t s_tx_end_page;
static uint32_t s_tx_started_ms;

static uint8_t oled_pages_to_mask(uint8_t start_page, uint8_t end_page);

static uint8_t oled_wait_i2c_flag_set(uint32_t i2c_periph, i2c_flag_enum flag, uint32_t timeout)
{
    while (timeout-- != 0U) {
        if (SET == i2c_flag_get(i2c_periph, flag)) {
            return 1U;
        }
    }

    return 0U;
}

static uint8_t oled_wait_i2c_stop_clear(uint32_t i2c_periph, uint32_t timeout)
{
    while (timeout-- != 0U) {
        if (0U == (I2C_CTL0(i2c_periph) & I2C_CTL0_STOP)) {
            return 1U;
        }
    }

    return 0U;
}

static uint8_t oled_wait_dma_ftf(uint32_t timeout)
{
    while (timeout-- != 0U) {
        if (SET == dma_flag_get(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_FTF)) {
            return 1U;
        }
    }

    return 0U;
}

static uint8_t oled_wait_i2c_tx_complete(uint32_t timeout)
{
    while (timeout-- != 0U) {
        if (SET == i2c_flag_get(OLED_I2C_PERIPH, I2C_FLAG_AERR)) {
            i2c_flag_clear(OLED_I2C_PERIPH, I2C_FLAG_AERR);
            return OLED_ERR;
        }

        if (SET == i2c_flag_get(OLED_I2C_PERIPH, I2C_FLAG_BTC)) {
            return OLED_OK;
        }
    }

    return OLED_TIMEOUT;
}

static uint8_t oled_i2c_tx_status(void)
{
    if (SET == i2c_flag_get(OLED_I2C_PERIPH, I2C_FLAG_AERR)) {
        i2c_flag_clear(OLED_I2C_PERIPH, I2C_FLAG_AERR);
        return OLED_ERR;
    }

    if (SET == i2c_flag_get(OLED_I2C_PERIPH, I2C_FLAG_BTC)) {
        return OLED_OK;
    }

    return OLED_BUSY;
}

static uint8_t oled_wait_addsend_or_nack(uint32_t timeout)
{
    while (timeout-- != 0U) {
        if (SET == i2c_flag_get(OLED_I2C_PERIPH, I2C_FLAG_ADDSEND)) {
            return 1U;
        }

        if (SET == i2c_flag_get(OLED_I2C_PERIPH, I2C_FLAG_AERR)) {
            i2c_flag_clear(OLED_I2C_PERIPH, I2C_FLAG_AERR);
            return 0U;
        }
    }

    return 0U;
}

static void oled_dma_int_enable_all(void)
{
    dma_interrupt_enable(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FTF);
    dma_interrupt_enable(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_TAE);
    dma_interrupt_enable(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_SDE);
    dma_interrupt_enable(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FEE);
}

static void oled_dma_int_disable_all(void)
{
    dma_interrupt_disable(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FTF);
    dma_interrupt_disable(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_TAE);
    dma_interrupt_disable(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_SDE);
    dma_interrupt_disable(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FEE);
}

static void oled_dma_int_clear_all(void)
{
    dma_interrupt_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_FTF);
    dma_interrupt_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_TAE);
    dma_interrupt_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_SDE);
    dma_interrupt_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_FEE);
}

static void oled_i2c_bus_reset(void)
{
    uint8_t i;

    i2c_dma_config(OLED_I2C_PERIPH, I2C_DMA_OFF);
    dma_channel_disable(OLED_DMA_PERIPH, OLED_DMA_CH);

    gpio_mode_set(OLED_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, OLED_SCL_PIN | OLED_SDA_PIN);
    gpio_output_options_set(OLED_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, OLED_SCL_PIN | OLED_SDA_PIN);

    gpio_bit_set(OLED_GPIO_PORT, OLED_SCL_PIN | OLED_SDA_PIN);
    for (i = 0U; i < 9U; i++) {
        gpio_bit_reset(OLED_GPIO_PORT, OLED_SCL_PIN);
        gpio_bit_set(OLED_GPIO_PORT, OLED_SCL_PIN);
    }

    gpio_bit_set(OLED_GPIO_PORT, OLED_SCL_PIN);
    gpio_bit_reset(OLED_GPIO_PORT, OLED_SDA_PIN);
    gpio_bit_set(OLED_GPIO_PORT, OLED_SDA_PIN);

    gpio_af_set(OLED_GPIO_PORT, GPIO_AF_4, OLED_SDA_PIN | OLED_SCL_PIN);
    gpio_mode_set(OLED_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OLED_SDA_PIN | OLED_SCL_PIN);
    gpio_output_options_set(OLED_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, OLED_SDA_PIN | OLED_SCL_PIN);

    i2c_deinit(OLED_I2C_PERIPH);
    i2c_clock_config(OLED_I2C_PERIPH, 400000U, I2C_DTCY_2);
    i2c_mode_addr_config(OLED_I2C_PERIPH, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, OLED_I2C_OWN_ADDRESS);
    i2c_enable(OLED_I2C_PERIPH);
    i2c_ack_config(OLED_I2C_PERIPH, I2C_ACK_ENABLE);
}

static void oled_mark_pages(uint8_t top, uint8_t bottom)
{
    uint8_t page;
    uint8_t start_page;
    uint8_t end_page;

    if (top >= OLED_HEIGHT) {
        return;
    }
    if (bottom >= OLED_HEIGHT) {
        bottom = OLED_HEIGHT - 1U;
    }
    if (top > bottom) {
        return;
    }

    start_page = (uint8_t)(top / 8U);
    end_page = (uint8_t)(bottom / 8U);
    for (page = start_page; page <= end_page; page++) {
        s_dirty_pages |= (uint8_t)(1U << page);
    }
}

static void oled_mark_all_dirty(void)
{
    s_dirty_pages = OLED_ALL_PAGES;
}

static uint8_t oled_prepare_i2c(uint8_t addr)
{
    uint32_t timeout = 10000U;

    if (addr != OLED_I2C_WRITE_ADDR) {
        return OLED_ERR;
    }

    if (SET == i2c_flag_get(OLED_I2C_PERIPH, I2C_FLAG_I2CBSY)) {
        oled_i2c_bus_reset();
    }

    while ((SET == i2c_flag_get(OLED_I2C_PERIPH, I2C_FLAG_I2CBSY)) && (--timeout != 0U)) {
    }

    if (timeout == 0U) {
        oled_i2c_bus_reset();
        timeout = 10000U;
        while ((SET == i2c_flag_get(OLED_I2C_PERIPH, I2C_FLAG_I2CBSY)) && (--timeout != 0U)) {
        }
        if (timeout == 0U) {
            return OLED_TIMEOUT;
        }
    }

    i2c_start_on_bus(OLED_I2C_PERIPH);
    if (oled_wait_i2c_flag_set(OLED_I2C_PERIPH, I2C_FLAG_SBSEND, OLED_I2C_WAIT_TIMEOUT) == 0U) {
        oled_i2c_bus_reset();
        return OLED_TIMEOUT;
    }

    i2c_master_addressing(OLED_I2C_PERIPH, addr, I2C_TRANSMITTER);
    if (oled_wait_addsend_or_nack(OLED_I2C_WAIT_TIMEOUT) == 0U) {
        i2c_stop_on_bus(OLED_I2C_PERIPH);
        (void)oled_wait_i2c_stop_clear(OLED_I2C_PERIPH, OLED_I2C_WAIT_TIMEOUT / 10U);
        oled_i2c_bus_reset();
        return OLED_ERR;
    }

    i2c_flag_clear(OLED_I2C_PERIPH, I2C_FLAG_ADDSEND);
    if (oled_wait_i2c_flag_set(OLED_I2C_PERIPH, I2C_FLAG_TBE, OLED_I2C_WAIT_TIMEOUT) == 0U) {
        oled_i2c_bus_reset();
        return OLED_TIMEOUT;
    }

    return OLED_OK;
}

static void oled_stop_i2c_dma(void)
{
    oled_dma_int_disable_all();
    dma_channel_disable(OLED_DMA_PERIPH, OLED_DMA_CH);
    i2c_dma_config(OLED_I2C_PERIPH, I2C_DMA_OFF);
    i2c_dma_last_transfer_config(OLED_I2C_PERIPH, I2C_DMALST_OFF);
}

static uint8_t oled_current_window_pages(void)
{
    if (s_tx_phase == OLED_TX_IDLE) {
        return 0U;
    }

    return oled_pages_to_mask(s_tx_start_page, s_tx_end_page);
}

static void oled_tx_finish(void)
{
    oled_stop_i2c_dma();
    i2c_stop_on_bus(OLED_I2C_PERIPH);
}

static uint8_t oled_tx_finish_blocking(void)
{
    uint8_t res;

    oled_stop_i2c_dma();
    res = oled_wait_i2c_tx_complete(OLED_I2C_WAIT_TIMEOUT);
    i2c_stop_on_bus(OLED_I2C_PERIPH);

    if (oled_wait_i2c_stop_clear(OLED_I2C_PERIPH, OLED_I2C_WAIT_TIMEOUT) == 0U) {
        oled_i2c_bus_reset();
        return OLED_TIMEOUT;
    }

    if (res != OLED_OK) {
        oled_i2c_bus_reset();
    }

    return res;
}

static uint8_t oled_dma_start(uint8_t addr, uint8_t control, uint8_t *buf, uint16_t len, uint8_t irq)
{
    uint8_t res;

    if ((buf == NULL) || (len == 0U) || (len > (OLED_TX_BUF_SIZE - 1U))) {
        return OLED_ERR;
    }

    res = oled_prepare_i2c(addr);
    if (res != OLED_OK) {
        return res;
    }

    s_oled_tx_buf[0] = control;
    if (buf != &s_oled_tx_buf[1]) {
        (void)memcpy(&s_oled_tx_buf[1], buf, len);
    }

    dma_channel_disable(OLED_DMA_PERIPH, OLED_DMA_CH);
    dma_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_FTF);
    dma_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_TAE);
    dma_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_SDE);
    dma_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_FEE);
    oled_dma_int_clear_all();
    dma_memory_address_config(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_MEMORY_0, (uint32_t)s_oled_tx_buf);
    dma_transfer_number_config(OLED_DMA_PERIPH, OLED_DMA_CH, (uint32_t)len + 1U);
    i2c_dma_last_transfer_config(OLED_I2C_PERIPH, I2C_DMALST_ON);
    i2c_dma_config(OLED_I2C_PERIPH, I2C_DMA_ON);

    if (irq != 0U) {
        oled_dma_int_enable_all();
    } else {
        oled_dma_int_disable_all();
    }

    dma_channel_enable(OLED_DMA_PERIPH, OLED_DMA_CH);

    return OLED_OK;
}

static uint8_t oled_dma_write(uint8_t addr, uint8_t control, uint8_t *buf, uint16_t len)
{
    uint8_t res;

    if (s_oled_busy != 0U) {
        return OLED_BUSY;
    }

    res = oled_dma_start(addr, control, buf, len, 0U);
    if (res != OLED_OK) {
        return res;
    }

    if (oled_wait_dma_ftf(OLED_I2C_WAIT_TIMEOUT) == 0U) {
        oled_stop_i2c_dma();
        i2c_stop_on_bus(OLED_I2C_PERIPH);
        (void)oled_wait_i2c_stop_clear(OLED_I2C_PERIPH, OLED_I2C_WAIT_TIMEOUT / 10U);
        oled_i2c_bus_reset();
        return OLED_TIMEOUT;
    }

    dma_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_FTF);
    return oled_tx_finish_blocking();
}

static uint8_t oled_iic_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(OLED_GPIO_CLOCK);
    rcu_periph_clock_enable(OLED_I2C_CLOCK);
    rcu_periph_clock_enable(OLED_DMA_CLOCK);

    gpio_af_set(OLED_GPIO_PORT, GPIO_AF_4, OLED_SDA_PIN | OLED_SCL_PIN);
    gpio_mode_set(OLED_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OLED_SDA_PIN | OLED_SCL_PIN);
    gpio_output_options_set(OLED_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, OLED_SDA_PIN | OLED_SCL_PIN);

    i2c_deinit(OLED_I2C_PERIPH);
    i2c_clock_config(OLED_I2C_PERIPH, 400000U, I2C_DTCY_2);
    i2c_mode_addr_config(OLED_I2C_PERIPH, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, OLED_I2C_OWN_ADDRESS);
    i2c_enable(OLED_I2C_PERIPH);
    i2c_ack_config(OLED_I2C_PERIPH, I2C_ACK_ENABLE);

    dma_deinit(OLED_DMA_PERIPH, OLED_DMA_CH);
    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.memory0_addr = (uint32_t)s_oled_tx_buf;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_addr = OLED_I2C_DATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.number = 1U;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(OLED_DMA_PERIPH, OLED_DMA_CH, &dma_init_struct);
    dma_circulation_disable(OLED_DMA_PERIPH, OLED_DMA_CH);
    dma_channel_subperipheral_select(OLED_DMA_PERIPH, OLED_DMA_CH, OLED_DMA_SUBPERI);

    nvic_irq_enable(OLED_DMA_IRQn, 2U, 0U);

    return OLED_OK;
}

static uint8_t oled_iic_deinit(void)
{
    oled_stop_i2c_dma();
    i2c_deinit(OLED_I2C_PERIPH);

    return OLED_OK;
}

static uint8_t oled_noop_init(void)
{
    return OLED_OK;
}

static uint8_t oled_noop_write(uint8_t value)
{
    (void)value;

    return OLED_OK;
}

static uint8_t oled_noop_spi_write(uint8_t *buf, uint16_t len)
{
    (void)buf;
    (void)len;

    return OLED_OK;
}

static void oled_delay_ms(uint32_t ms)
{
    delay_1ms(ms);
}

static void oled_debug_print(const char *const fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    (void)vprintf(fmt, args);
    va_end(args);
}

static void oled_link_handle(void)
{
    DRIVER_SSD1306_LINK_INIT(&s_oled_handle, ssd1306_handle_t);
    DRIVER_SSD1306_LINK_IIC_INIT(&s_oled_handle, oled_iic_init);
    DRIVER_SSD1306_LINK_IIC_DEINIT(&s_oled_handle, oled_iic_deinit);
    DRIVER_SSD1306_LINK_IIC_WRITE(&s_oled_handle, oled_dma_write);
    DRIVER_SSD1306_LINK_SPI_INIT(&s_oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_SPI_DEINIT(&s_oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_SPI_WRITE_COMMAND(&s_oled_handle, oled_noop_spi_write);
    DRIVER_SSD1306_LINK_SPI_COMMAND_DATA_GPIO_INIT(&s_oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_SPI_COMMAND_DATA_GPIO_DEINIT(&s_oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_SPI_COMMAND_DATA_GPIO_WRITE(&s_oled_handle, oled_noop_write);
    DRIVER_SSD1306_LINK_RESET_GPIO_INIT(&s_oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_RESET_GPIO_DEINIT(&s_oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_RESET_GPIO_WRITE(&s_oled_handle, oled_noop_write);
    DRIVER_SSD1306_LINK_DELAY_MS(&s_oled_handle, oled_delay_ms);
    DRIVER_SSD1306_LINK_DEBUG_PRINT(&s_oled_handle, oled_debug_print);
}

static uint8_t oled_write_cmds(const uint8_t *cmds, uint8_t len)
{
    return ssd1306_write_cmd(&s_oled_handle, (uint8_t *)cmds, len);
}

static uint8_t oled_config_128x32(void)
{
    static const uint8_t init_cmds[] = {
        0xAEU,
        0xD5U, 0x80U,
        0xA8U, 0x1FU,
        0xD3U, 0x00U,
        0x40U,
        0x8DU, 0x14U,
        0x20U, 0x00U,
        0xA1U,
        0xC8U,
        0xDAU, 0x00U,
        0x81U, 0x80U,
        0xD9U, 0x1FU,
        0xDBU, 0x40U,
        0xA4U,
        0xA6U,
        0xAFU,
    };

    return oled_write_cmds(init_cmds, (uint8_t)sizeof(init_cmds));
}

static void oled_find_window(uint8_t mask, uint8_t *start_page, uint8_t *end_page)
{
    uint8_t page;

    *start_page = 0U;
    *end_page = 0U;

    for (page = 0U; page < OLED_PAGE_COUNT; page++) {
        if ((mask & (1U << page)) != 0U) {
            *start_page = page;
            *end_page = page;
            while (((*end_page + 1U) < OLED_PAGE_COUNT) &&
                   ((mask & (1U << (*end_page + 1U))) != 0U)) {
                (*end_page)++;
            }
            return;
        }
    }
}

static uint8_t oled_pages_to_mask(uint8_t start_page, uint8_t end_page)
{
    uint8_t mask = 0U;
    uint8_t page;

    for (page = start_page; page <= end_page; page++) {
        mask |= (uint8_t)(1U << page);
    }

    return mask;
}

static void oled_prepare_window_cmd(uint8_t start_page, uint8_t end_page)
{
    s_oled_cmd_buf[0] = 0x21U;
    s_oled_cmd_buf[1] = 0x00U;
    s_oled_cmd_buf[2] = (uint8_t)(OLED_WIDTH - 1U);
    s_oled_cmd_buf[3] = 0x22U;
    s_oled_cmd_buf[4] = start_page;
    s_oled_cmd_buf[5] = end_page;
}

static uint16_t oled_prepare_window_data(uint8_t start_page, uint8_t end_page)
{
    uint16_t len = 0U;
    uint8_t page;
    uint8_t x;

    for (page = start_page; page <= end_page; page++) {
        for (x = 0U; x < OLED_WIDTH; x++) {
            s_oled_cmd_buf[0] = s_oled_handle.gram[x][page];
            s_oled_tx_buf[1U + len] = s_oled_cmd_buf[0];
            len++;
        }
    }

    return len;
}

static uint8_t oled_start_window_cmd(uint8_t start_page, uint8_t end_page)
{
    uint8_t res;

    s_tx_start_page = start_page;
    s_tx_end_page = end_page;
    s_tx_phase = OLED_TX_CMD;
    s_tx_dma_done = 0U;
    s_tx_dma_error = 0U;
    s_tx_started_ms = systick_get_ms();
    oled_prepare_window_cmd(start_page, end_page);

    res = oled_dma_start(OLED_I2C_WRITE_ADDR, 0x00U, s_oled_cmd_buf, 6U, 1U);
    if (res != OLED_OK) {
        s_tx_phase = OLED_TX_IDLE;
    }

    return res;
}

static uint8_t oled_start_window_data(void)
{
    uint8_t res;
    uint16_t len;

    len = oled_prepare_window_data(s_tx_start_page, s_tx_end_page);
    s_tx_phase = OLED_TX_DATA;
    s_tx_dma_done = 0U;
    s_tx_dma_error = 0U;
    s_tx_started_ms = systick_get_ms();

    res = oled_dma_start(OLED_I2C_WRITE_ADDR, 0x40U, &s_oled_tx_buf[1], len, 1U);
    if (res != OLED_OK) {
        s_tx_phase = OLED_TX_IDLE;
    }

    return res;
}

static uint8_t oled_update_dirty_sync(void)
{
    uint8_t res;
    uint8_t start_page;
    uint8_t end_page;
    uint8_t pages;
    uint16_t len;

    if (s_oled_busy != 0U) {
        res = oled_wait_ready(OLED_SYNC_TIMEOUT_MS);
        if (res != OLED_OK) {
            return res;
        }
    }

    s_oled_error = OLED_OK;
    while (s_dirty_pages != 0U) {
        oled_find_window(s_dirty_pages, &start_page, &end_page);
        pages = oled_pages_to_mask(start_page, end_page);
        s_dirty_pages &= (uint8_t)~pages;

        oled_prepare_window_cmd(start_page, end_page);
        res = oled_dma_write(OLED_I2C_WRITE_ADDR, 0x00U, s_oled_cmd_buf, 6U);
        if (res != OLED_OK) {
            s_dirty_pages |= pages;
            s_oled_error = res;
            return res;
        }

        len = oled_prepare_window_data(start_page, end_page);
        res = oled_dma_write(OLED_I2C_WRITE_ADDR, 0x40U, &s_oled_tx_buf[1], len);
        if (res != OLED_OK) {
            s_dirty_pages |= pages;
            s_oled_error = res;
            return res;
        }
    }

    s_active_pages = 0U;
    s_tx_phase = OLED_TX_IDLE;
    return OLED_OK;
}

static void oled_async_fail(uint8_t pages, uint8_t error)
{
    oled_tx_finish();
    (void)oled_wait_i2c_stop_clear(OLED_I2C_PERIPH, OLED_I2C_WAIT_TIMEOUT / 10U);
    oled_i2c_bus_reset();
    s_dirty_pages |= pages;
    s_active_pages = 0U;
    s_tx_phase = OLED_TX_IDLE;
    s_tx_dma_done = 0U;
    s_tx_dma_error = 0U;
    s_oled_error = error;
    s_oled_busy = 0U;
}

static uint8_t oled_start_next_window(void)
{
    uint8_t start_page;
    uint8_t end_page;
    uint8_t pages;

    if (s_active_pages == 0U) {
        s_tx_phase = OLED_TX_IDLE;
        s_oled_busy = 0U;
        return OLED_OK;
    }

    oled_find_window(s_active_pages, &start_page, &end_page);
    pages = oled_pages_to_mask(start_page, end_page);
    s_active_pages &= (uint8_t)~pages;

    if (oled_start_window_cmd(start_page, end_page) != OLED_OK) {
        oled_async_fail((uint8_t)(pages | s_active_pages), OLED_ERR);
        return OLED_ERR;
    }

    return OLED_OK;
}

static void oled_dma_poll_flags(void)
{
    if ((s_oled_busy == 0U) || (s_tx_dma_done != 0U) || (s_tx_dma_error != 0U)) {
        return;
    }

    if ((SET == dma_flag_get(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_TAE)) ||
        (SET == dma_flag_get(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_SDE)) ||
        (SET == dma_flag_get(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_FEE))) {
        dma_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_TAE);
        dma_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_SDE);
        dma_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_FEE);
        oled_stop_i2c_dma();
        s_tx_dma_error = 1U;
        return;
    }

    if (SET == dma_flag_get(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_FTF)) {
        dma_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_FLAG_FTF);
        oled_stop_i2c_dma();
        s_tx_dma_done = 1U;
    }
}

static uint8_t oled_async_poll(void)
{
    uint8_t res;
    uint8_t pages;

    if (s_oled_busy == 0U) {
        return (s_oled_error == OLED_OK) ? OLED_OK : s_oled_error;
    }

    pages = (uint8_t)(s_active_pages | oled_current_window_pages());
    oled_dma_poll_flags();

    if (s_tx_dma_error != 0U) {
        oled_async_fail(pages, OLED_ERR);
        return OLED_ERR;
    }

    if (s_tx_dma_done == 0U) {
        if ((uint32_t)(systick_get_ms() - s_tx_started_ms) >= OLED_ASYNC_TIMEOUT_MS) {
            oled_async_fail(pages, OLED_TIMEOUT);
            return OLED_TIMEOUT;
        }

        return OLED_BUSY;
    }

    res = oled_i2c_tx_status();
    if (res == OLED_BUSY) {
        return OLED_BUSY;
    }
    if (res != OLED_OK) {
        oled_async_fail(pages, res);
        return res;
    }

    i2c_stop_on_bus(OLED_I2C_PERIPH);
    if (oled_wait_i2c_stop_clear(OLED_I2C_PERIPH, OLED_I2C_WAIT_TIMEOUT) == 0U) {
        oled_async_fail(pages, OLED_TIMEOUT);
        return OLED_TIMEOUT;
    }

    s_tx_dma_done = 0U;
    if (s_tx_phase == OLED_TX_CMD) {
        if (oled_start_window_data() != OLED_OK) {
            oled_async_fail(pages, OLED_ERR);
            return OLED_ERR;
        }

        return OLED_BUSY;
    }

    s_tx_phase = OLED_TX_IDLE;
    return oled_start_next_window();
}

static void oled_draw_char_6x8(uint8_t x, uint8_t page, uint8_t ch, uint8_t color)
{
    uint8_t col;
    uint8_t chr = ch;

    if ((chr < ' ') || (chr > '~')) {
        chr = ' ';
    }
    chr = (uint8_t)(chr - ' ');

    for (col = 0U; col < 6U; col++) {
        if ((x + col) >= OLED_WIDTH) {
            return;
        }
        s_oled_handle.gram[x + col][page] = (color != 0U) ? F6X8[chr][col] : (uint8_t)~F6X8[chr][col];
    }
}

static void oled_draw_char_8x16(uint8_t x, uint8_t page, uint8_t ch, uint8_t color)
{
    uint8_t col;
    uint8_t chr = ch;

    if ((chr < ' ') || (chr > '~')) {
        chr = ' ';
    }
    chr = (uint8_t)(chr - ' ');

    for (col = 0U; col < 8U; col++) {
        if ((x + col) >= OLED_WIDTH) {
            return;
        }
        s_oled_handle.gram[x + col][page] = (color != 0U) ? F8X16[(chr * 16U) + col] : (uint8_t)~F8X16[(chr * 16U) + col];
        if ((page + 1U) < OLED_PAGE_COUNT) {
            s_oled_handle.gram[x + col][page + 1U] = (color != 0U) ? F8X16[(chr * 16U) + col + 8U] : (uint8_t)~F8X16[(chr * 16U) + col + 8U];
        }
    }
}

uint8_t oled_init(void)
{
    uint8_t res;

    oled_link_handle();
    res = ssd1306_set_interface(&s_oled_handle, SSD1306_INTERFACE_IIC);
    if (res != OLED_OK) {
        return OLED_ERR;
    }

    res = ssd1306_set_addr_pin(&s_oled_handle, SSD1306_ADDR_SA0_0);
    if (res != OLED_OK) {
        return OLED_ERR;
    }

    res = ssd1306_init(&s_oled_handle);
    if (res != OLED_OK) {
        return OLED_ERR;
    }

    res = oled_config_128x32();
    if (res != OLED_OK) {
        return OLED_ERR;
    }

    s_oled_inited = 1U;
    s_oled_busy = 0U;
    s_oled_error = OLED_OK;
    s_tx_phase = OLED_TX_IDLE;
    s_dirty_pages = 0U;
    (void)memset(s_oled_handle.gram, 0, sizeof(s_oled_handle.gram));
    oled_mark_all_dirty();

    return oled_update();
}

uint8_t oled_deinit(void)
{
    uint8_t res;

    if (s_oled_inited == 0U) {
        return OLED_OK;
    }

    (void)oled_wait_ready(OLED_ASYNC_TIMEOUT_MS);
    res = ssd1306_deinit(&s_oled_handle);
    s_oled_inited = 0U;
    s_dirty_pages = 0U;
    s_active_pages = 0U;
    s_oled_busy = 0U;
    s_tx_phase = OLED_TX_IDLE;

    return (res == 0U) ? OLED_OK : OLED_ERR;
}

uint8_t oled_clear(void)
{
    if (s_oled_inited == 0U) {
        return OLED_ERR;
    }

    (void)memset(s_oled_handle.gram, 0, sizeof(s_oled_handle.gram));
    oled_mark_all_dirty();

    return OLED_OK;
}

uint8_t oled_update_async(void)
{
    if (s_oled_inited == 0U) {
        return OLED_ERR;
    }

    return oled_update_dirty_sync();
}

uint8_t oled_update(void)
{
    if (s_oled_inited == 0U) {
        return OLED_ERR;
    }

    return oled_update_dirty_sync();
}

uint8_t oled_is_busy(void)
{
    if (s_oled_busy != 0U) {
        (void)oled_async_poll();
    }

    return s_oled_busy;
}

uint8_t oled_has_dirty(void)
{
    if (s_oled_busy != 0U) {
        (void)oled_async_poll();
    }

    return (s_dirty_pages != 0U) ? 1U : 0U;
}

uint8_t oled_wait_ready(uint32_t timeout_ms)
{
    uint32_t start = systick_get_ms();
    uint8_t res;

    while (s_oled_busy != 0U) {
        res = oled_async_poll();
        if ((res != OLED_BUSY) && (res != OLED_OK)) {
            return res;
        }

        if ((uint32_t)(systick_get_ms() - start) >= timeout_ms) {
            oled_async_fail((uint8_t)(s_active_pages | oled_current_window_pages()), OLED_TIMEOUT);
            return OLED_TIMEOUT;
        }
    }

    return (s_oled_error == OLED_OK) ? OLED_OK : OLED_ERR;
}

uint8_t oled_display_on(void)
{
    uint8_t res;

    if (s_oled_inited == 0U) {
        return OLED_ERR;
    }
    res = oled_wait_ready(OLED_ASYNC_TIMEOUT_MS);
    if (res != OLED_OK) {
        return res;
    }

    return (ssd1306_set_display(&s_oled_handle, SSD1306_DISPLAY_ON) == 0U) ? OLED_OK : OLED_ERR;
}

uint8_t oled_display_off(void)
{
    uint8_t res;

    if (s_oled_inited == 0U) {
        return OLED_ERR;
    }
    res = oled_wait_ready(OLED_ASYNC_TIMEOUT_MS);
    if (res != OLED_OK) {
        return res;
    }

    return (ssd1306_set_display(&s_oled_handle, SSD1306_DISPLAY_OFF) == 0U) ? OLED_OK : OLED_ERR;
}

uint8_t oled_draw_point(uint8_t x, uint8_t y, uint8_t color)
{
    uint8_t page;
    uint8_t bit;

    if ((s_oled_inited == 0U) || (x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
        return OLED_ERR;
    }

    page = (uint8_t)(y / 8U);
    bit = (uint8_t)(1U << (y % 8U));
    if (color != 0U) {
        s_oled_handle.gram[x][page] |= bit;
    } else {
        s_oled_handle.gram[x][page] &= (uint8_t)~bit;
    }
    oled_mark_pages(y, y);

    return OLED_OK;
}

uint8_t oled_fill_rect(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom, uint8_t color)
{
    uint8_t x;
    uint8_t y;

    if ((s_oled_inited == 0U) || (left >= OLED_WIDTH) || (top >= OLED_HEIGHT) || (left > right) || (top > bottom)) {
        return OLED_ERR;
    }
    if (right >= OLED_WIDTH) {
        right = OLED_WIDTH - 1U;
    }
    if (bottom >= OLED_HEIGHT) {
        bottom = OLED_HEIGHT - 1U;
    }

    for (y = top; y <= bottom; y++) {
        for (x = left; x <= right; x++) {
            (void)oled_draw_point(x, y, color);
        }
    }
    oled_mark_pages(top, bottom);

    return OLED_OK;
}

uint8_t oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t font, uint8_t color)
{
    uint8_t page;
    uint8_t char_w;
    uint8_t char_h;
    uint8_t cur_x;
    uint8_t end_y;
    uint8_t any = 0U;

    if ((s_oled_inited == 0U) || (str == NULL) || (x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
        return OLED_ERR;
    }
    if ((y % 8U) != 0U) {
        return OLED_ERR;
    }

    if (font == OLED_FONT_8) {
        char_w = 6U;
        char_h = 8U;
    } else if (font == OLED_FONT_16) {
        char_w = 8U;
        char_h = 16U;
    } else {
        return OLED_ERR;
    }

    if ((y + char_h) > OLED_HEIGHT) {
        return OLED_ERR;
    }

    page = (uint8_t)(y / 8U);
    cur_x = x;
    while (*str != '\0') {
        if ((cur_x + char_w) > OLED_WIDTH) {
            break;
        }

        if (font == OLED_FONT_8) {
            oled_draw_char_6x8(cur_x, page, (uint8_t)*str, color);
        } else {
            oled_draw_char_8x16(cur_x, page, (uint8_t)*str, color);
        }
        cur_x = (uint8_t)(cur_x + char_w);
        str++;
        any = 1U;
    }

    if (any == 0U) {
        return OLED_ERR;
    }

    end_y = (uint8_t)(y + char_h - 1U);
    oled_mark_pages(y, end_y);

    return OLED_OK;
}

int oled_printf(uint8_t x, uint8_t y, const char *format, ...)
{
    char buffer[128];
    va_list arg;
    int len;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (len < 0) {
        return len;
    }

    if (oled_show_string(x, y, buffer, OLED_FONT_8, 1U) != OLED_OK) {
        return -1;
    }

    return len;
}

void oled_dma_irq_handler(void)
{
    if (s_oled_busy == 0U) {
        oled_dma_int_clear_all();
        return;
    }

    if ((SET == dma_interrupt_flag_get(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_TAE)) ||
        (SET == dma_interrupt_flag_get(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_SDE)) ||
        (SET == dma_interrupt_flag_get(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_FEE))) {
        dma_interrupt_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_TAE);
        dma_interrupt_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_SDE);
        dma_interrupt_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_FEE);
        oled_stop_i2c_dma();
        s_tx_dma_error = 1U;
        return;
    }

    if (SET == dma_interrupt_flag_get(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_FTF)) {
        dma_interrupt_flag_clear(OLED_DMA_PERIPH, OLED_DMA_CH, DMA_INT_FLAG_FTF);
        oled_stop_i2c_dma();
        s_tx_dma_done = 1U;
    }
}
