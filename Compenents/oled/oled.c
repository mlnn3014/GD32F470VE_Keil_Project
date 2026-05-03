#include "oled.h"

#include "driver_ssd1306.h"
#include "oled_bsp.h"
#include "oledfont.h"
#include "systick.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define OLED_IIC_ADDR        0x78U
#define OLED_PAGE_COUNT      (OLED_HEIGHT / 8U)
#define OLED_ALL_PAGES       ((uint8_t)((1U << OLED_PAGE_COUNT) - 1U))
#define OLED_CMD_BUF_SIZE    7U
#define OLED_SYNC_TIMEOUT_MS 200U

static ssd1306_handle_t oled_handle;
static uint8_t oled_inited;
static uint8_t oled_cmd_buf[OLED_CMD_BUF_SIZE];
static uint8_t oled_data_buf[OLED_WIDTH * OLED_PAGE_COUNT];
static volatile uint8_t oled_dirty_pages;
static uint8_t oled_busy;
static uint8_t oled_error;
static uint8_t oled_next_page;

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

static void oled_link_handle(void)
{
    DRIVER_SSD1306_LINK_INIT(&oled_handle, ssd1306_handle_t);
    DRIVER_SSD1306_LINK_IIC_INIT(&oled_handle, oled_bsp_iic_init);
    DRIVER_SSD1306_LINK_IIC_DEINIT(&oled_handle, oled_bsp_iic_deinit);
    DRIVER_SSD1306_LINK_IIC_WRITE(&oled_handle, oled_bsp_iic_write);
    DRIVER_SSD1306_LINK_SPI_INIT(&oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_SPI_DEINIT(&oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_SPI_WRITE_COMMAND(&oled_handle, oled_noop_spi_write);
    DRIVER_SSD1306_LINK_SPI_COMMAND_DATA_GPIO_INIT(&oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_SPI_COMMAND_DATA_GPIO_DEINIT(&oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_SPI_COMMAND_DATA_GPIO_WRITE(&oled_handle, oled_noop_write);
    DRIVER_SSD1306_LINK_RESET_GPIO_INIT(&oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_RESET_GPIO_DEINIT(&oled_handle, oled_noop_init);
    DRIVER_SSD1306_LINK_RESET_GPIO_WRITE(&oled_handle, oled_noop_write);
    DRIVER_SSD1306_LINK_DELAY_MS(&oled_handle, oled_bsp_delay_ms);
    DRIVER_SSD1306_LINK_DEBUG_PRINT(&oled_handle, oled_bsp_debug_print);
}

static uint8_t oled_write_cmds(const uint8_t *cmds, uint8_t len)
{
    return ssd1306_write_cmd(&oled_handle, (uint8_t *)cmds, len);
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

static uint8_t oled_pages_to_mask(uint8_t start_page, uint8_t end_page)
{
    uint8_t page;
    uint8_t mask = 0U;

    for (page = start_page; page <= end_page; page++) {
        mask |= (uint8_t)(1U << page);
    }

    return mask;
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
        oled_dirty_pages |= (uint8_t)(1U << page);
    }
}

static void oled_mark_all_dirty(void)
{
    oled_dirty_pages = OLED_ALL_PAGES;
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

static void oled_find_window_from(uint8_t mask, uint8_t first_page, uint8_t *start_page, uint8_t *end_page)
{
    uint8_t i;
    uint8_t page;

    *start_page = 0U;
    *end_page = 0U;

    for (i = 0U; i < OLED_PAGE_COUNT; i++) {
        page = (uint8_t)((first_page + i) % OLED_PAGE_COUNT);
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

static void oled_prepare_window_cmd(uint8_t start_page, uint8_t end_page)
{
    oled_cmd_buf[0] = 0x21U;
    oled_cmd_buf[1] = 0x00U;
    oled_cmd_buf[2] = (uint8_t)(OLED_WIDTH - 1U);
    oled_cmd_buf[3] = 0x22U;
    oled_cmd_buf[4] = start_page;
    oled_cmd_buf[5] = end_page;
}

static uint16_t oled_prepare_window_data(uint8_t start_page, uint8_t end_page)
{
    uint8_t page;
    uint8_t x;
    uint16_t len = 0U;

    for (page = start_page; page <= end_page; page++) {
        for (x = 0U; x < OLED_WIDTH; x++) {
            oled_data_buf[len] = oled_handle.gram[x][page];
            len++;
        }
    }

    return len;
}

static uint8_t oled_flush_window(uint8_t start_page, uint8_t end_page)
{
    uint8_t res;
    uint16_t len;

    oled_prepare_window_cmd(start_page, end_page);
    res = oled_bsp_iic_write(OLED_IIC_ADDR, 0x00U, oled_cmd_buf, 6U);
    if (res != OLED_OK) {
        return res;
    }

    len = oled_prepare_window_data(start_page, end_page);

    return oled_bsp_iic_write(OLED_IIC_ADDR, 0x40U, oled_data_buf, len);
}

static uint8_t oled_update_dirty_sync(void)
{
    uint8_t res;
    uint8_t start_page;
    uint8_t end_page;
    uint8_t pages;

    if (oled_busy != 0U) {
        res = oled_wait_ready(OLED_SYNC_TIMEOUT_MS);
        if (res != OLED_OK) {
            return res;
        }
    }

    oled_error = OLED_OK;
    while (oled_dirty_pages != 0U) {
        oled_find_window(oled_dirty_pages, &start_page, &end_page);
        pages = oled_pages_to_mask(start_page, end_page);
        oled_dirty_pages &= (uint8_t)~pages;

        res = oled_flush_window(start_page, end_page);
        if (res != OLED_OK) {
            oled_dirty_pages |= pages;
            oled_error = res;
            return res;
        }
    }

    oled_next_page = 0U;

    return OLED_OK;
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
        oled_handle.gram[x + col][page] = (color != 0U) ? F6X8[chr][col] : (uint8_t)~F6X8[chr][col];
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
        oled_handle.gram[x + col][page] = (color != 0U) ?
                                          F8X16[(chr * 16U) + col] :
                                          (uint8_t)~F8X16[(chr * 16U) + col];
        if ((page + 1U) < OLED_PAGE_COUNT) {
            oled_handle.gram[x + col][page + 1U] = (color != 0U) ?
                                                   F8X16[(chr * 16U) + col + 8U] :
                                                   (uint8_t)~F8X16[(chr * 16U) + col + 8U];
        }
    }
}

uint8_t oled_init(void)
{
    uint8_t res;

    oled_link_handle();
    res = ssd1306_set_interface(&oled_handle, SSD1306_INTERFACE_IIC);
    if (res != 0U) {
        return OLED_ERR;
    }

    res = ssd1306_set_addr_pin(&oled_handle, SSD1306_ADDR_SA0_0);
    if (res != 0U) {
        return OLED_ERR;
    }

    res = ssd1306_init(&oled_handle);
    if (res != 0U) {
        return OLED_ERR;
    }

    res = oled_config_128x32();
    if (res != 0U) {
        return OLED_ERR;
    }

    oled_inited = 1U;
    oled_busy = 0U;
    oled_error = OLED_OK;
    oled_dirty_pages = 0U;
    oled_next_page = 0U;
    (void)memset(oled_handle.gram, 0, sizeof(oled_handle.gram));
    oled_mark_all_dirty();

    return oled_update();
}

uint8_t oled_deinit(void)
{
    uint8_t res;

    if (oled_inited == 0U) {
        return OLED_OK;
    }

    (void)oled_wait_ready(OLED_SYNC_TIMEOUT_MS);
    res = ssd1306_deinit(&oled_handle);
    oled_inited = 0U;
    oled_dirty_pages = 0U;
    oled_busy = 0U;
    oled_next_page = 0U;

    return (res == 0U) ? OLED_OK : OLED_ERR;
}

uint8_t oled_clear(void)
{
    if (oled_inited == 0U) {
        return OLED_ERR;
    }

    (void)memset(oled_handle.gram, 0, sizeof(oled_handle.gram));
    oled_mark_all_dirty();

    return OLED_OK;
}

uint8_t oled_update(void)
{
    if (oled_inited == 0U) {
        return OLED_ERR;
    }

    return oled_update_dirty_sync();
}

uint8_t oled_update_async(void)
{
    uint8_t res;
    uint8_t pages;
    uint8_t start_page;
    uint8_t end_page;

    if (oled_inited == 0U) {
        return OLED_ERR;
    }
    if (oled_busy != 0U) {
        return OLED_BUSY;
    }
    if (oled_dirty_pages == 0U) {
        return OLED_OK;
    }

    oled_find_window_from(oled_dirty_pages, oled_next_page, &start_page, &end_page);
    pages = oled_pages_to_mask(start_page, end_page);
    oled_dirty_pages &= (uint8_t)~pages;
    oled_busy = 1U;
    oled_error = OLED_OK;

    res = oled_flush_window(start_page, end_page);
    oled_busy = 0U;
    oled_next_page = (uint8_t)((end_page + 1U) % OLED_PAGE_COUNT);
    if (res != OLED_OK) {
        oled_dirty_pages |= pages;
        oled_error = res;
        return res;
    }

    return (oled_dirty_pages != 0U) ? OLED_BUSY : OLED_OK;
}

uint8_t oled_is_busy(void)
{
    return oled_busy;
}

uint8_t oled_has_dirty(void)
{
    return (oled_dirty_pages != 0U) ? 1U : 0U;
}

uint8_t oled_wait_ready(uint32_t timeout_ms)
{
    uint32_t start = systick_get_ms();

    while (oled_busy != 0U) {
        if ((uint32_t)(systick_get_ms() - start) >= timeout_ms) {
            return OLED_TIMEOUT;
        }
    }

    return (oled_error == OLED_OK) ? OLED_OK : OLED_ERR;
}

uint8_t oled_display_on(void)
{
    uint8_t res;

    if (oled_inited == 0U) {
        return OLED_ERR;
    }

    res = oled_wait_ready(OLED_SYNC_TIMEOUT_MS);
    if (res != OLED_OK) {
        return res;
    }

    return (ssd1306_set_display(&oled_handle, SSD1306_DISPLAY_ON) == 0U) ? OLED_OK : OLED_ERR;
}

uint8_t oled_display_off(void)
{
    uint8_t res;

    if (oled_inited == 0U) {
        return OLED_ERR;
    }

    res = oled_wait_ready(OLED_SYNC_TIMEOUT_MS);
    if (res != OLED_OK) {
        return res;
    }

    return (ssd1306_set_display(&oled_handle, SSD1306_DISPLAY_OFF) == 0U) ? OLED_OK : OLED_ERR;
}

uint8_t oled_draw_point(uint8_t x, uint8_t y, uint8_t color)
{
    uint8_t page;
    uint8_t bit;

    if ((oled_inited == 0U) || (x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
        return OLED_ERR;
    }

    page = (uint8_t)(y / 8U);
    bit = (uint8_t)(1U << (y % 8U));
    if (color != 0U) {
        oled_handle.gram[x][page] |= bit;
    } else {
        oled_handle.gram[x][page] &= (uint8_t)~bit;
    }
    oled_mark_pages(y, y);

    return OLED_OK;
}

uint8_t oled_fill_rect(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom, uint8_t color)
{
    uint8_t x;
    uint8_t y;

    if ((oled_inited == 0U) || (left >= OLED_WIDTH) || (top >= OLED_HEIGHT) ||
        (left > right) || (top > bottom)) {
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

    if ((oled_inited == 0U) || (str == NULL) || (x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
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
