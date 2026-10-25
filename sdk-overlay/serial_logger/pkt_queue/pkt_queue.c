#include <stdlib.h>
#include <string.h>

#include "ql_api_osi.h"
#include "pkt_queue.h"

struct pkt_queue
{
    ql_queue_t q;
    unsigned int pkt_size;
};

pkt_queue_t *pkt_queue_init(unsigned int pkt_size, unsigned int max_count)
{
    pkt_queue_t *q;

    if (pkt_size == 0 || max_count == 0)
        return NULL;

    q = calloc(1, sizeof(*q));
    if (q == NULL)
        return NULL;

    q->pkt_size = pkt_size;

    if (ql_rtos_queue_create(&q->q, pkt_size, max_count) != QL_OSI_SUCCESS)
    {
        free(q);
        return NULL;
    }

    return q;
}

void pkt_queue_deinit(pkt_queue_t *q)
{
    if (q == NULL)
        return;

    if (q->q != NULL)
        ql_rtos_queue_delete(q->q);

    free(q);
}

int pkt_queue_post(pkt_queue_t *q, const void *pkt)
{
    if (q == NULL || pkt == NULL)
        return -1;

    if (ql_rtos_queue_release(q->q, q->pkt_size, (uint8_t *)pkt, QL_NO_WAIT) != QL_OSI_SUCCESS)
        return -1;

    return 0;
}

int pkt_queue_recv(pkt_queue_t *q, void *pkt, uint32_t timeout_ms)
{
    if (q == NULL || pkt == NULL)
        return -1;

    if (ql_rtos_queue_wait(q->q, (uint8_t *)pkt, q->pkt_size, timeout_ms) != QL_OSI_SUCCESS)
        return -1;

    return 0;
}
