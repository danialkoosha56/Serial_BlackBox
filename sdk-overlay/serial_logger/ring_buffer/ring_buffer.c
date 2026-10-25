#include <stdlib.h>
#include <string.h>

#include "ring_buffer.h"

#define MIN(a, b)  ((a) < (b) ? (a) : (b))

struct ring_buffer
{
    unsigned char *buff;
    volatile unsigned int wp;
    volatile unsigned int rp;
    volatile unsigned int length;
    unsigned int size;
};

ring_buffer_t *ring_buffer_init(unsigned int size)
{
    ring_buffer_t *rb;

    if (size == 0)
        return NULL;

    rb = calloc(1, sizeof(*rb));
    if (rb == NULL)
        return NULL;

    rb->buff = calloc(1, size);
    if (rb->buff == NULL)
    {
        free(rb);
        return NULL;
    }

    rb->size = size;
    return rb;
}

void ring_buffer_deinit(ring_buffer_t *rb)
{
    if (rb == NULL)
        return;

    free(rb->buff);
    free(rb);
}

unsigned int ring_buffer_write(ring_buffer_t *rb, const unsigned char *data, unsigned int len)
{
    unsigned int a, b;

    if (rb == NULL || data == NULL || rb->length + len > rb->size)
        return 0;

    if (rb->size - rb->wp < len)
    {
        a = rb->size - rb->wp;
        b = len - a;
    }
    else
    {
        a = len;
        b = 0;
    }

    if (b)
    {
        memcpy(rb->buff + rb->wp, data, a);
        memcpy(rb->buff, data + a, b);
        rb->wp = b;
    }
    else
    {
        memcpy(rb->buff + rb->wp, data, a);
        rb->wp += a;
        if (rb->wp == rb->size)
            rb->wp = 0;
    }

    rb->length += len;
    return len;
}

unsigned int ring_buffer_read(ring_buffer_t *rb, unsigned char *data, unsigned int len)
{
    unsigned int n, a, b;

    if (rb == NULL || data == NULL)
        return 0;

    n = MIN(rb->length, len);
    if (rb->rp + n > rb->size)
    {
        a = rb->size - rb->rp;
        b = n - a;
    }
    else
    {
        a = n;
        b = 0;
    }

    if (b)
    {
        memcpy(data, rb->buff + rb->rp, a);
        memcpy(data + a, rb->buff, b);
        rb->rp = b;
    }
    else
    {
        memcpy(data, rb->buff + rb->rp, a);
        rb->rp += a;
        if (rb->rp == rb->size)
            rb->rp = 0;
    }

    rb->length -= n;
    return n;
}

unsigned int ring_buffer_length(const ring_buffer_t *rb)
{
    if (rb == NULL)
        return 0;

    return rb->length;
}
