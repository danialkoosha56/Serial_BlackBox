#include <stdlib.h>
#include <string.h>

#include "ql_api_osi.h"
#include "ql_uart.h"
#include "ql_pin_cfg.h"
#include "ql_app_feature_config.h"
#include "dani_log.h"
#include "ring_buffer.h"
#include "mqtt_proc.h"
#include "sd_proc.h"
#include "input_log_proc.h"

#define DANI_LOG_FRAME_ID  Window12
#define LOG(msg, ...)      DANI_LOG(DANI_LOG_FRAME_ID, QL_LOG_LEVEL_INFO, "dani_uart", msg, ##__VA_ARGS__)
#define TIMER_INTERVAL     10000 //ms
#define RX_SIZE            2048
#define RB_SIZE            4096
#define RB_HIGH_WATER      ((RB_SIZE * 3) / 4)
#define MIN(a, b)          ((a) < (b) ? (a) : (b))

static ring_buffer_t *serial_rb = NULL;
static unsigned char rx_buf[RX_SIZE + 1];
static ql_task_t s_task = NULL;
static ql_timer_t proc_timer = NULL;
static ql_sem_t proc_sem = NULL;

static void proc_timer_cb(void *ctx)
{
    (void)ctx;
    LOG("timer tick %u ms -> task", TIMER_INTERVAL);
    ql_rtos_semaphore_release(proc_sem);
}

static void uart_notify_cb(unsigned int ind_type, ql_uart_port_number_e port, unsigned int size)
{
    int read_len = 0;
    unsigned int total_rx = 0;
    unsigned int rb_len;

    if (serial_rb == NULL)
        return;

    if (ind_type != QUEC_UART_RX_OVERFLOW_IND && ind_type != QUEC_UART_RX_RECV_DATA_IND)
        return;

    while (size > 0)
    {
        unsigned int chunk = MIN(size, RX_SIZE);

        memset(rx_buf, 0, sizeof(rx_buf));
        read_len = ql_uart_read(port, rx_buf, chunk);
        if (read_len <= 0)
            break;

        ring_buffer_write(serial_rb, rx_buf, (unsigned int)read_len);
        total_rx += (unsigned int)read_len;
        size -= (unsigned int)read_len;
    }

    if (total_rx == 0)
        return;

    rb_len = ring_buffer_length(serial_rb);
    LOG("rx %u rb %u/%u", total_rx, rb_len, RB_SIZE);

    if (rb_len >= RB_HIGH_WATER)
    {
        LOG("rb >=75%% -> task");
        ql_rtos_semaphore_release(proc_sem);
    }
}

static void INPUT_LOG_PROCESS(void *param)
{
    LOG("%s", __func__);
    LOG("=======================================");
    LOG("this window porccess all the input log from the serial port\r\n");

    ql_uart_config_s cfg = {0};
    unsigned int n;

    (void)param;

    if (ql_rtos_semaphore_create_ex(&proc_sem, 0, 1) != QL_OSI_SUCCESS)
    {
        LOG("init fail: sem");
        return;
    }
    LOG("init ok: sem");

    /* callback must NOT run on this task while it waits on proc_sem */
    if (ql_rtos_timer_create(&proc_timer, QL_TIMER_IN_SERVICE, proc_timer_cb, NULL) != QL_OSI_SUCCESS)
    {
        LOG("init fail: timer");
        return;
    }
    LOG("init ok: timer");

    serial_rb = ring_buffer_init(RB_SIZE);
    if (serial_rb == NULL)
    {
        LOG("init fail: rb %u", RB_SIZE);
        return;
    }
    LOG("init ok: rb %u", RB_SIZE);

    cfg.baudrate = QL_UART_BAUD_115200;
    cfg.flow_ctrl = QL_FC_NONE;
    cfg.data_bit = QL_UART_DATABIT_8;
    cfg.stop_bit = QL_UART_STOP_1;
    cfg.parity_bit = QL_UART_PARITY_NONE;

    if (ql_uart_set_dcbconfig(QL_UART_PORT_1, &cfg) != QL_UART_SUCCESS ||
        ql_uart_open(QL_UART_PORT_1) != QL_UART_SUCCESS ||
        ql_uart_register_cb(QL_UART_PORT_1, uart_notify_cb) != QL_UART_SUCCESS)
    {
        LOG("init fail: uart1");
        return;
    }
    LOG("init ok: uart1 ready");

    if (ql_rtos_timer_start(proc_timer, TIMER_INTERVAL, 1) != QL_OSI_SUCCESS)
    {
        LOG("init fail: timer start");
        return;
    }
    LOG("init ok: timer every %u ms", TIMER_INTERVAL);

    while (1)
    {
        ql_rtos_semaphore_wait(proc_sem, QL_WAIT_FOREVER);

        n = ring_buffer_length(serial_rb);
        if (n == 0)
        {
            LOG("task wake rb empty\r\n\r\n\r\n");
            continue;
        }

        if (n > RX_SIZE)
            n = RX_SIZE;

        {
            log_pkt_t log_pkt = {0};

            if (n > UART_PKT_DATA_SIZE)
                n = UART_PKT_DATA_SIZE;

            ring_buffer_read(serial_rb, log_pkt.data, n);
            log_pkt.len = n;
            log_pkt.time_or_size = false;

            LOG("task send %u bytes", log_pkt.len);
            LOG("proc_buf: %s", log_pkt.data);
            mqtt_uart_post(&log_pkt);
            sd_log_post(&log_pkt);
            LOG("\r\n\r\n\r\n");
        }
    }
}

void dani_input_log_proc_app_init(void)
{
    /* 16KB stack: Quectel uart_rb demo uses 4096*4; 4KB often fails (heap/stack). */
    if (ql_rtos_task_create(&s_task, 4096 * 4, APP_PRIORITY_NORMAL, "IN_LOG_PROC",
                            INPUT_LOG_PROCESS, NULL, 5) != QL_OSI_SUCCESS)
    {
        LOG("init fail: task");
        return;
    }

}

