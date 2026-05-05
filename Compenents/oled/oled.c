#include "oled.h"

#include "oled_bsp.h"
#include "oledfont.h"
#include "systick.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define OLED_PAGE_COUNT      (OLED_HEIGHT / 8U)
#define OLED_ALL_PAGES       ((uint8_t)((1U << OLED_PAGE_COUNT) - 1U))
#define OLED_CMD_BUF_SIZE    7U
#define OLED_SYNC_TIMEOUT_MS 200U

typedef struct {
    uint8_t char_w;
    uint8_t char_h;
    uint8_t rows;
    uint8_t cols;
} oled_font_info_t;

static uint8_t oled_inited;
static uint8_t oled_gram[OLED_WIDTH][OLED_PAGE_COUNT];
static uint8_t oled_cmd_buf[OLED_CMD_BUF_SIZE];
static uint8_t oled_data_buf[OLED_WIDTH * OLED_PAGE_COUNT];
static volatile uint8_t oled_dirty_pages;
static uint8_t oled_busy;
static uint8_t oled_error;
static uint8_t oled_next_page;

static uint8_t oled_wait_ready(uint32_t timeout_ms);

static uint8_t oled_get_font_info(uint8_t font, oled_font_info_t *info)
{
    if (info == NULL) {
        return OLED_ERR;
    }

    if (font == OLED_FONT_8) {
        info->char_w = 6U;
        info->char_h = 8U;
    } else if (font == OLED_FONT_16) {
        info->char_w = 8U;
        info->char_h = 16U;
    } else {
        return OLED_ERR;
    }

    info->rows = (uint8_t)(OLED_HEIGHT / info->char_h);
    info->cols = (uint8_t)(OLED_WIDTH / info->char_w);

    return OLED_OK;
}

static uint8_t oled_text_to_rect(uint8_t font, uint8_t row, uint8_t col, uint8_t cols,
                                 uint8_t *x, uint8_t *y, uint8_t *w, uint8_t *h)
{
    oled_font_info_t info;

    if ((x == NULL) || (y == NULL) || (w == NULL) || (h == NULL)) {
        return OLED_ERR;
    }
    if (oled_get_font_info(font, &info) != OLED_OK) {
        return OLED_ERR;
    }
    if ((row >= info.rows) || (col >= info.cols)) {
        return OLED_ERR;
    }

    *x = (uint8_t)(col * info.char_w);
    *y = (uint8_t)(row * info.char_h);
    if ((cols == 0U) || (cols > (uint8_t)(info.cols - col))) {
        *w = (uint8_t)(OLED_WIDTH - *x);
    } else {
        *w = (uint8_t)(cols * info.char_w);
    }
    *h = info.char_h;

    return OLED_OK;
}

static uint8_t oled_write_cmds(const uint8_t *cmds, uint8_t len)
{
    if ((cmds == NULL) || (len == 0U)) {
        return OLED_ERR;
    }

    return oled_bus_write(0x00U, cmds, len);
}

static uint8_t oled_write_cmd(uint8_t cmd)
{
    return oled_write_cmds(&cmd, 1U);
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
            oled_data_buf[len] = oled_gram[x][page];
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
    res = oled_bus_write(0x00U, oled_cmd_buf, 6U);
    if (res != OLED_OK) {
        return res;
    }

    len = oled_prepare_window_data(start_page, end_page);

    return oled_bus_write(0x40U, oled_data_buf, len);
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
        oled_gram[x + col][page] = (color != 0U) ? F6X8[chr][col] : (uint8_t)~F6X8[chr][col];
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
        oled_gram[x + col][page] = (color != 0U) ?
                                   F8X16[(chr * 16U) + col] :
                                   (uint8_t)~F8X16[(chr * 16U) + col];
        if ((page + 1U) < OLED_PAGE_COUNT) {
            oled_gram[x + col][page + 1U] = (color != 0U) ?
                                            F8X16[(chr * 16U) + col + 8U] :
                                            (uint8_t)~F8X16[(chr * 16U) + col + 8U];
        }
    }
}

uint8_t oled_init(void)
{
    uint8_t res;

    res = oled_bus_init();
    if (res != OLED_OK) {
        return res;
    }

    res = oled_config_128x32();
    if (res != OLED_OK) {
        return OLED_ERR;
    }

    oled_inited = 1U;
    oled_busy = 0U;
    oled_error = OLED_OK;
    oled_dirty_pages = 0U;
    oled_next_page = 0U;
    (void)memset(oled_gram, 0, sizeof(oled_gram));
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
    (void)oled_write_cmd(0xAEU);
    res = oled_bus_deinit();
    oled_inited = 0U;
    oled_dirty_pages = 0U;
    oled_busy = 0U;
    oled_next_page = 0U;

    return (res == OLED_OK) ? OLED_OK : OLED_ERR;
}

uint8_t oled_clear(void)
{
    if (oled_inited == 0U) {
        return OLED_ERR;
    }

    (void)memset(oled_gram, 0, sizeof(oled_gram));
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

uint8_t oled_service(void)
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

static uint8_t oled_wait_ready(uint32_t timeout_ms)
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

    return oled_write_cmd(0xAFU);
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

    return oled_write_cmd(0xAEU);
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
        oled_gram[x][page] |= bit;
    } else {
        oled_gram[x][page] &= (uint8_t)~bit;
    }
    oled_mark_pages(y, y);

    return OLED_OK;
}

uint8_t oled_fill_rect(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom, uint8_t color)
{
    uint8_t page;
    uint8_t x;
    uint8_t start_page;
    uint8_t end_page;
    uint8_t top_bit;
    uint8_t bottom_bit;
    uint8_t mask;

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

    start_page = (uint8_t)(top / 8U);
    end_page = (uint8_t)(bottom / 8U);

    for (page = start_page; page <= end_page; page++) {
        top_bit = (page == start_page) ? (uint8_t)(top % 8U) : 0U;
        bottom_bit = (page == end_page) ? (uint8_t)(bottom % 8U) : 7U;
        mask = (uint8_t)((0xFFU << top_bit) & (0xFFU >> (7U - bottom_bit)));

        for (x = left; x <= right; x++) {
            if (color != 0U) {
                oled_gram[x][page] |= mask;
            } else {
                oled_gram[x][page] &= (uint8_t)~mask;
            }
        }
    }
    oled_mark_pages(top, bottom);

    return OLED_OK;
}

uint8_t oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t font, uint8_t color)
{
    oled_font_info_t info;
    uint8_t page;
    uint8_t cur_x;
    uint8_t end_y;
    uint8_t any = 0U;

    if ((oled_inited == 0U) || (str == NULL) || (x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
        return OLED_ERR;
    }
    if ((y % 8U) != 0U) {
        return OLED_ERR;
    }
    if (oled_get_font_info(font, &info) != OLED_OK) {
        return OLED_ERR;
    }

    if ((y + info.char_h) > OLED_HEIGHT) {
        return OLED_ERR;
    }

    page = (uint8_t)(y / 8U);
    cur_x = x;
    while (*str != '\0') {
        if ((cur_x + info.char_w) > OLED_WIDTH) {
            break;
        }

        if (font == OLED_FONT_8) {
            oled_draw_char_6x8(cur_x, page, (uint8_t)*str, color);
        } else {
            oled_draw_char_8x16(cur_x, page, (uint8_t)*str, color);
        }
        cur_x = (uint8_t)(cur_x + info.char_w);
        str++;
        any = 1U;
    }

    if (any == 0U) {
        return OLED_ERR;
    }

    end_y = (uint8_t)(y + info.char_h - 1U);
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

uint8_t oled_text_clear(uint8_t font, uint8_t row, uint8_t col, uint8_t cols)
{
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;

    if (oled_text_to_rect(font, row, col, cols, &x, &y, &w, &h) != OLED_OK) {
        return OLED_ERR;
    }

    return oled_fill_rect(x, y, (uint8_t)(x + w - 1U), (uint8_t)(y + h - 1U), 0U);
}

uint8_t oled_text_show(uint8_t font, uint8_t row, uint8_t col, uint8_t cols, const char *str)
{
    oled_font_info_t info;
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;
    uint8_t show_cols;
    char buffer[22];

    if (str == NULL) {
        return OLED_ERR;
    }
    if (oled_get_font_info(font, &info) != OLED_OK) {
        return OLED_ERR;
    }
    if (oled_text_to_rect(font, row, col, cols, &x, &y, &w, &h) != OLED_OK) {
        return OLED_ERR;
    }

    show_cols = (uint8_t)(w / info.char_w);
    if (show_cols >= sizeof(buffer)) {
        show_cols = (uint8_t)(sizeof(buffer) - 1U);
    }

    (void)oled_fill_rect(x, y, (uint8_t)(x + w - 1U), (uint8_t)(y + h - 1U), 0U);
    (void)strncpy(buffer, str, show_cols);
    buffer[show_cols] = '\0';

    if (buffer[0] == '\0') {
        return OLED_OK;
    }

    return oled_show_string(x, y, buffer, font, 1U);
}

int oled_text_printf(uint8_t font, uint8_t row, uint8_t col, uint8_t cols, const char *format, ...)
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
    if (oled_text_show(font, row, col, cols, buffer) != OLED_OK) {
        return -1;
    }

    return len;
}
