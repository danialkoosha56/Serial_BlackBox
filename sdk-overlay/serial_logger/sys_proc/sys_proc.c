#include "ql_api_osi.h"
#include "ql_gpio.h"
#include "ql_pin_cfg.h"
#include "dani_log.h"
#include "dani_sys.h"
#include "sys_proc.h"

#define DANI_LOG_FRAME_ID  Window11
#define LOG(msg, ...)      DANI_LOG(DANI_LOG_FRAME_ID, QL_LOG_LEVEL_INFO, "sys_proc", msg, ##__VA_ARGS__)
#define STATUS_INTERVAL    30

#define LED_BLINK_1S() \
    ql_gpio_set_level(QL_GPIO_NET_MODE, 1); \
    ql_rtos_task_sleep_ms(900); \
    ql_gpio_set_level(QL_GPIO_NET_MODE, 0); \
    ql_rtos_task_sleep_ms(100);

static ql_task_t s_task = NULL;
static uint32_t s_tick_sec = 0;

static void LOG_SYS_INFO(void)
{
    LOG("\r\n==============func: %s=============", __func__);
    LOG("sim status=%s(%d)", sys_sim_status_str(sys_info.sim_status), sys_info.sim_status);
    if (sys_info.iccid[0])
        LOG("sim iccid=%s", sys_info.iccid);
    if (sys_info.imsi[0])
        LOG("sim imsi=%s", sys_info.imsi);

    LOG("voice reg=%s \r\ndata reg=%s act=%d",
        sys_reg_state_str(sys_info.voice_reg),
        sys_reg_state_str(sys_info.data_reg),
        sys_info.act);
    LOG("nw csq=%u", sys_info.csq);
    if (sys_info.operator_name[0])
        LOG("nw oper=%s,  mcc=%s,  mnc=%s", sys_info.operator_name, sys_info.mcc, sys_info.mnc);

    LOG("data pdp=%s,  pidx=%d,  ip=%s,  dns=%s",
        sys_data_state_str(sys_info.data_state),
        sys_info.profile_idx,
        sys_info.ip[0] ? sys_info.ip : "-",
        sys_info.pri_dns[0] ? sys_info.pri_dns : "-");
    LOG("internet=%s", sys_internet_ready() ? "ready" : "down");

    if (sys_info.mqtt != NULL)
    {
        LOG("mqtt conn=%d sub=%d init=%d pub=%d rx=%d",
            sys_info.mqtt->connected,
            sys_info.mqtt->subscribed,
            sys_info.mqtt->cli_inited,
            sys_info.mqtt->published,
            sys_info.mqtt->received);
    }
    else
    {
        LOG("mqtt=n/a");
    }

    LOG("clock %04d-%02d-%02d %02d:%02d:%02d \r\nts=%ld",
        sys_info.rtc.tm_year, sys_info.rtc.tm_mon, sys_info.rtc.tm_mday,
        sys_info.rtc.tm_hour, sys_info.rtc.tm_min, sys_info.rtc.tm_sec,
        (long)sys_info.timestamp);
    LOG("lifetime %02u:%02u:%02u",
        sys_info.lifetime / 3600, (sys_info.lifetime / 60) % 60, sys_info.lifetime % 60);

    LOG("=====================================\r\n\r\n\r\n");
}

static void bootstrap(void)
{
    LOG("init start");

    ql_pin_set_func(QL_PIN_NET_MODE, QL_PIN_NET_MODE_FUNC_GPIO);
    ql_gpio_init(QL_GPIO_NET_MODE, GPIO_OUTPUT, PULL_NONE, LVL_LOW);

    if (sys_get() != NULL)
        LOG_SYS_INFO();

    if (sys_bootstrap() != 0)
        LOG("bootstrap fail");
    else
        LOG("bootstrap ok");

    if (sys_network_up() != 0)
        LOG("network_up fail");
    else
        LOG("network_up ok");

    if (sys_get() != NULL)
        LOG_SYS_INFO();

    LOG("init done");
}

static void sys_proc(void *param)
{
    uint32_t last_status = 0;

    (void)param;

    LOG("%s", __func__);
    LOG("===========================================");
    LOG("this window porccess the system information and the network status\r\n");

    bootstrap();

    while (1)
    {
        if (sys_get() != NULL)
        {
            if (s_tick_sec - last_status >= STATUS_INTERVAL)
            {
                LOG_SYS_INFO();
                last_status = s_tick_sec;
            }
            else
            {
                LOG("lifetime %u", sys_info.lifetime);
            }
        }

        LED_BLINK_1S();
        s_tick_sec++;
    }
}

void sys_proc_init(void)
{
    if (ql_rtos_task_create(&s_task, 4096 * 2, APP_PRIORITY_NORMAL, "sys_proc",
                            sys_proc, NULL, 1) != QL_OSI_SUCCESS)
        LOG("init fail: task");
}
