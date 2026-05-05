#include "ring_buffer.h"

static uint8_t ring_buffer_size_valid(uint16_t size)
{
    return ((size != 0U) && ((size & (uint16_t)(size - 1U)) == 0U)) ? 1U : 0U;
}

static uint16_t ring_buffer_next(const ring_buffer_t *ring, uint16_t index)
{
    return (uint16_t)((index + 1U) & ring->mask);
}

void ring_buffer_init(ring_buffer_t *ring, uint8_t *buf, uint16_t size)
{
    if (ring == 0) {
        return;
    }

    ring->buf = buf;
    ring->size = size;
    ring->mask = (uint16_t)(size - 1U);
    ring->read = 0U;
    ring->write = 0U;
    ring->count = 0U;

    if ((buf == 0) || (ring_buffer_size_valid(size) == 0U)) {
        ring->buf = 0;
        ring->size = 0U;
        ring->mask = 0U;
    }
}

uint16_t ring_buffer_write(ring_buffer_t *ring, const uint8_t *data, uint16_t len)
{
    uint16_t written = 0U;

    if ((ring == 0) || (ring->buf == 0) || (data == 0)) {
        return 0U;
    }

    while ((written < len) && (ring->count < ring->size)) {
        ring->buf[ring->write] = data[written];
        ring->write = ring_buffer_next(ring, ring->write);
        ring->count++;
        written++;
    }

    return written;
}

uint16_t ring_buffer_read(ring_buffer_t *ring, uint8_t *data, uint16_t len)
{
    uint16_t read_count = 0U;

    if ((ring == 0) || (ring->buf == 0) || (data == 0)) {
        return 0U;
    }

    while ((read_count < len) && (ring->count > 0U)) {
        data[read_count] = ring->buf[ring->read];
        ring->read = ring_buffer_next(ring, ring->read);
        ring->count--;
        read_count++;
    }

    return read_count;
}

uint16_t ring_buffer_available(const ring_buffer_t *ring)
{
    if (ring == 0) {
        return 0U;
    }

    return ring->count;
}

uint16_t ring_buffer_free(const ring_buffer_t *ring)
{
    if ((ring == 0) || (ring->count > ring->size)) {
        return 0U;
    }

    return (uint16_t)(ring->size - ring->count);
}

uint8_t ring_buffer_is_full(const ring_buffer_t *ring)
{
    if (ring == 0) {
        return 0U;
    }

    return (ring->count >= ring->size) ? 1U : 0U;
}

const uint8_t *ring_buffer_read_ptr(const ring_buffer_t *ring)
{
    if ((ring == 0) || (ring->buf == 0) || (ring->count == 0U)) {
        return 0;
    }

    return &ring->buf[ring->read];
}

uint16_t ring_buffer_read_linear(const ring_buffer_t *ring)
{
    uint16_t len;

    if ((ring == 0) || (ring->buf == 0) || (ring->count == 0U)) {
        return 0U;
    }

    len = ring->count;
    if ((uint32_t)ring->read + len > ring->size) {
        len = (uint16_t)(ring->size - ring->read);
    }

    return len;
}

void ring_buffer_drop(ring_buffer_t *ring, uint16_t len)
{
    if ((ring == 0) || (ring->buf == 0)) {
        return;
    }

    if (len > ring->count) {
        len = ring->count;
    }

    ring->read = (uint16_t)((ring->read + len) & ring->mask);
    ring->count = (uint16_t)(ring->count - len);
}
