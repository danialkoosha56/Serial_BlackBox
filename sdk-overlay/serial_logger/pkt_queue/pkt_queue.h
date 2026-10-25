#ifndef PKT_QUEUE_H
#define PKT_QUEUE_H

#include <stdint.h>

typedef struct pkt_queue pkt_queue_t;

/** @param pkt_size  sizeof() your packet struct */
pkt_queue_t *pkt_queue_init(unsigned int pkt_size, unsigned int max_count);
void pkt_queue_deinit(pkt_queue_t *q);

int pkt_queue_post(pkt_queue_t *q, const void *pkt);
int pkt_queue_recv(pkt_queue_t *q, void *pkt, uint32_t timeout_ms);

#endif /* PKT_QUEUE_H */
