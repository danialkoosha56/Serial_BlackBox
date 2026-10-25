#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ql_api_osi.h"
#include "ql_gpio.h"
#include "ql_adc.h"
#include "dani_log.h"
#include "io_proc.h"

#define DANI_LOG_FRAME_ID  Window24
#define LOG(msg, ...)      DANI_LOG(DANI_LOG_FRAME_ID, QL_LOG_LEVEL_INFO, "io_proc", msg, ##__VA_ARGS__)

#define IO_TASK_STACK          (4096 * 2)
#define IO_STATUS_INTERVAL_S   5

static ql_task_t s_task = NULL;
static bool s_ready = false;

static void io_log(char *log, size_t log_len, const char *fmt, ...)
{
    va_list ap;

    if (log == NULL || log_len == 0)
        return;

    va_start(ap, fmt);
    vsnprintf(log, log_len, fmt, ap);
    va_end(ap);
}

static int io_hw_init(void)
{
    ql_errcode_gpio err;

    err = ql_gpio_init(IO_PROC_OUTPUT_GPIO, GPIO_OUTPUT, PULL_NONE, LVL_LOW);
    if (err != QL_GPIO_SUCCESS)
    {
        LOG("output gpio init fail %d", err);
        return -1;
    }

    err = ql_gpio_init(IO_PROC_INPUT_GPIO, GPIO_INPUT, PULL_NONE, LVL_LOW);
    if (err != QL_GPIO_SUCCESS)
    {
        LOG("input gpio init fail %d", err);
        return -1;
    }

    s_ready = true;
    LOG("output=gpio%d input=gpio%d adc=ch%d",
        (int)IO_PROC_OUTPUT_GPIO, (int)IO_PROC_INPUT_GPIO, IO_PROC_ADC_CHANNEL);
    return 0;
}

static void io_print_status(void)
{
    ql_LvlMode out_lvl = LVL_LOW;
    ql_LvlMode in_lvl = LVL_LOW;
    int mv = 0;
    int adc_ok;

    if (!s_ready)
        return;

    if (ql_gpio_get_level(IO_PROC_OUTPUT_GPIO, &out_lvl) != QL_GPIO_SUCCESS)
        out_lvl = LVL_LOW;

    if (ql_gpio_get_level(IO_PROC_INPUT_GPIO, &in_lvl) != QL_GPIO_SUCCESS)
        in_lvl = LVL_LOW;

    adc_ok = (ql_adc_get_volt(IO_PROC_ADC_CHANNEL, &mv) == QL_ADC_SUCCESS);

    LOG("out=%s in=%s adc=%d mV",
        (out_lvl == LVL_HIGH) ? "high" : "low",
        (in_lvl == LVL_HIGH) ? "high" : "low",
        adc_ok ? mv : -1);
}

static void io_proc_task(void *param)
{
    (void)param;

    LOG("%s", __func__);

    while (1)
    {
        io_print_status();
        ql_rtos_task_sleep_s(IO_STATUS_INTERVAL_S);
    }
}

void io_proc_init(void)
{
    if (s_task != NULL)
        return;

    io_hw_init();

    if (ql_rtos_task_create(&s_task, IO_TASK_STACK, APP_PRIORITY_NORMAL, "io_proc",
                            io_proc_task, NULL, 1) != QL_OSI_SUCCESS)
        LOG("init fail: task");
}

int io_proc_set_output(bool high, char *log, size_t log_len)
{
    ql_LvlMode lvl = high ? LVL_HIGH : LVL_LOW;

    if (!s_ready)
    {
        io_log(log, log_len, "io not ready");
        return -1;
    }

    if (ql_gpio_set_level(IO_PROC_OUTPUT_GPIO, lvl) != QL_GPIO_SUCCESS)
    {
        io_log(log, log_len, "set output fail");
        return -1;
    }

    io_log(log, log_len, "out=%s", high ? "high" : "low");
    return 0;
}

int io_proc_read_input(int *level, char *log, size_t log_len)
{
    ql_LvlMode lvl = LVL_LOW;

    if (!s_ready)
    {
        io_log(log, log_len, "io not ready");
        return -1;
    }

    if (ql_gpio_get_level(IO_PROC_INPUT_GPIO, &lvl) != QL_GPIO_SUCCESS)
    {
        io_log(log, log_len, "read input fail");
        return -1;
    }

    if (level != NULL)
        *level = (lvl == LVL_HIGH) ? 1 : 0;

    io_log(log, log_len, "in=%d", (lvl == LVL_HIGH) ? 1 : 0);
    return 0;
}

int io_proc_read_adc(int *millivolt, char *log, size_t log_len)
{
    int mv = 0;

    if (!s_ready)
    {
        io_log(log, log_len, "io not ready");
        return -1;
    }

    if (ql_adc_get_volt(IO_PROC_ADC_CHANNEL, &mv) != QL_ADC_SUCCESS)
    {
        io_log(log, log_len, "read adc fail");
        return -1;
    }

    if (millivolt != NULL)
        *millivolt = mv;

    io_log(log, log_len, "adc=%d mV", mv);
    return 0;
}
