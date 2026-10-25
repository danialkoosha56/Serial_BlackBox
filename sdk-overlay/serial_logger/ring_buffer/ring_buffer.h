#ifndef RING_BUFFER_H
#define RING_BUFFER_H

typedef struct ring_buffer ring_buffer_t;

ring_buffer_t *ring_buffer_init(unsigned int size);
void ring_buffer_deinit(ring_buffer_t *rb);

unsigned int ring_buffer_write(ring_buffer_t *rb, const unsigned char *data, unsigned int len);
unsigned int ring_buffer_read(ring_buffer_t *rb, unsigned char *data, unsigned int len);
unsigned int ring_buffer_length(const ring_buffer_t *rb);

#endif /* RING_BUFFER_H */
