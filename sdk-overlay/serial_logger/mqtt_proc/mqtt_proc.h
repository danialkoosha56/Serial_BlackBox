#ifndef MQTT_PROC_H
#define MQTT_PROC_H

#include <stdbool.h>
#include <stdint.h>

#define UART_PKT_DATA_SIZE  2048
#define ANS_DATA_SIZE  256

typedef struct
{
    unsigned char data[UART_PKT_DATA_SIZE];
    unsigned int len;
    unsigned int start_time;
    unsigned int end_time;
    bool time_or_size; /* true: timestamp, false: size */
} log_pkt_t;

typedef struct
{
    unsigned char data[UART_PKT_DATA_SIZE];
    unsigned int len;
} ans_pkt_t;

int dani_mqtt_app_init(void);
int mqtt_uart_post(const log_pkt_t *log_pkt);
int mqtt_answer_post(const ans_pkt_t *answer_pkt);

#endif /* MQTT_PROC_H */
