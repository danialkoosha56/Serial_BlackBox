#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "ql_api_osi.h"
#include "ql_uart.h"
#include "ql_pin_cfg.h"
#include "dani_log.h"
#include "command.h"

ql_uart_port_number_e ePort = QL_UART_PORT_2;

static char s_body[DANI_LOG_BUF_SIZE];
static char s_out[DANI_LOG_BUF_SIZE];
static ql_mutex_t s_mutex = NULL;
static int s_ready = 0;

static void dani_log_lock(void)
{
    if (!s_ready)
        dani_log_init();

    if (s_mutex == NULL)
        ql_rtos_mutex_create(&s_mutex);
    if (s_mutex != NULL)
        ql_rtos_mutex_lock(s_mutex, QL_WAIT_FOREVER);
}

static void dani_log_unlock(void)
{
    if (s_mutex != NULL)
        ql_rtos_mutex_unlock(s_mutex);
}

void ql_uart_notify_cb(unsigned int ind_type, ql_uart_port_number_e port, unsigned int size)
{
    unsigned char buf[CMD_MAX_LEN] = {0};

    if (ind_type != QUEC_UART_RX_RECV_DATA_IND && ind_type != QUEC_UART_RX_OVERFLOW_IND)
        return;

    if ( port != ePort)
        return;
    
    if (size >= CMD_MAX_LEN)
        size = CMD_MAX_LEN-1;

    ql_uart_read(port, buf, size);

    DANI_LOG(Window12, QL_LOG_LEVEL_INFO, NULL, "recived text:");
    DANI_LOG(Window12, QL_LOG_LEVEL_INFO, NULL, "%s", (const char *)buf);

    cmd_procc(CMD_SRC_UART, (const unsigned char *)buf, size);
}


int dani_log_init(void)
{
    ql_uart_config_s cfg = {0};

    if (s_ready)
        return 0;

    cfg.baudrate = QL_UART_BAUD_115200;
    cfg.flow_ctrl = QL_FC_NONE;
    cfg.data_bit = QL_UART_DATABIT_8;
    cfg.stop_bit = QL_UART_STOP_1;
    cfg.parity_bit = QL_UART_PARITY_NONE;

    if (ql_uart_set_dcbconfig(ePort, &cfg) != QL_UART_SUCCESS)
        return -1;

    ql_pin_set_func(QL_UART2_TX_PIN, QL_UART2_TX_FUNC);
    ql_pin_set_func(QL_UART2_RX_PIN, QL_UART2_RX_FUNC);

    if (ql_uart_open(ePort) != QL_UART_SUCCESS)
        return -1;

    ql_uart_register_cb(ePort, ql_uart_notify_cb);

    if (s_mutex == NULL)
        ql_rtos_mutex_create(&s_mutex);

    s_ready = 1;
    return 0;
}

#if DANI_LOG_USE_FRAME

int dani_log_impl_framed(unsigned char frame_id, const char *fmt, ...)
{
    va_list ap;
    int body_len;
    int write_len = 0;
    unsigned int pkt_len;

    dani_log_lock();

    va_start(ap, fmt);
    body_len = vsnprintf(s_body, sizeof(s_body) - 3, fmt, ap);
    va_end(ap);

    if (body_len > 0 && s_ready)
    {
        s_out[0] = DANI_LOG_FRAME_START;
        s_out[1] = (unsigned char)(DANI_LOG_FRAME_START + frame_id);
        memcpy(&s_out[2], s_body, (size_t)body_len);
        s_out[2 + body_len] = DANI_LOG_FRAME_END;

        pkt_len = (unsigned int)(2 + body_len + 1);
        write_len = ql_uart_write(ePort, (unsigned char *)s_out, pkt_len);
    }

    dani_log_unlock();
    return write_len;
}

#else

int dani_log_impl(int level, const char *module_str, const char *func,
                  int line, const char *fmt, ...)
{
    va_list ap;
    int body_len;
    int out_len;
    int write_len = 0;

    (void)level;

    dani_log_lock();

    va_start(ap, fmt);
    body_len = vsnprintf(s_body, sizeof(s_body), fmt, ap);
    va_end(ap);

    if (body_len >= 0)
    {
        out_len = snprintf(s_out, sizeof(s_out), "%s:%s %d %s\r\n",
                           module_str, func, line, s_body);
        if (out_len > 0 && s_ready)
            write_len = ql_uart_write(ePort, (unsigned char *)s_out, (unsigned int)out_len);
    }

    dani_log_unlock();
    return write_len;
}

#endif
