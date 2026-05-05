#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buf;
    uint16_t size;
    uint16_t mask;
    uint16_t read;
    uint16_t write;
    uint16_t count;
} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *ring, uint8_t *buf, uint16_t size);
uint16_t ring_buffer_write(ring_buffer_t *ring, const uint8_t *data, uint16_t len);
uint16_t ring_buffer_read(ring_buffer_t *ring, uint8_t *data, uint16_t len);
uint16_t ring_buffer_available(const ring_buffer_t *ring);
uint16_t ring_buffer_free(const ring_buffer_t *ring);
uint8_t ring_buffer_is_full(const ring_buffer_t *ring);
const uint8_t *ring_buffer_read_ptr(const ring_buffer_t *ring);
uint16_t ring_buffer_read_linear(const ring_buffer_t *ring);
void ring_buffer_drop(ring_buffer_t *ring, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
