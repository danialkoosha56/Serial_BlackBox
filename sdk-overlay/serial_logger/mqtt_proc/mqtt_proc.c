#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "ql_api_osi.h"
#include "ql_api_datacall.h"
#include "ql_mqttclient.h"
#include "dani_log.h"
#include "dani_sys.h"
#include "pkt_queue.h"
#include "command.h"
#include "mqtt_proc.h"

#define MQTT_UART_MAX_PKTS  8

static pkt_queue_t *uart2mqtt_q;
static pkt_queue_t *answer2mqtt_q;

#define DANI_LOG_FRAME_ID  Window13
#define LOG(msg, ...)      DANI_LOG(DANI_LOG_FRAME_ID, QL_LOG_LEVEL_INFO, "dani_mqtt", msg, ##__VA_ARGS__)

#define MQTT_BROKER_URL     "mqtt://test.mosquitto.org:1883"
#define MQTT_LOG_PUB_TOPIC      "my/test/uart/data"
#define MQTT_CMD_SUB_TOPIC      "my/test/uart/cmd"
#define MQTT_ANSWER_PUB_TOPIC   "my/test/uart/answer"
#define MQTT_CLIENT_ID_BASE "ec200_dani"
#define MQTT_PROFILE_IDX    SYS_PROFILE_IDX_DEFAULT
#define MQTT_SIM_ID         SYS_SIM_ID

#define SEMA_CREATE()  ql_rtos_semaphore_create_ex(&s_sem, 0, 1)
#define SEMA_DELETE()  ql_rtos_semaphore_delete(s_sem)
#define SEMA_RELEASE() ql_rtos_semaphore_release(s_sem)
#define SEMA_WAIT()    ql_rtos_semaphore_wait(s_sem, QL_WAIT_FOREVER)

static ql_task_t s_task = NULL;
static ql_sem_t s_sem = NULL;
static volatile int s_async_err = 0;
static mqtt_stat_t mqtt_stat = {0};
static char s_client_id[32];

static bool mqtt_is_connected(void)
{
    return mqtt_stat.connected;
}

static void mqtt_exception_cb(mqtt_client_t *client)
{
    (void)client;
    LOG("%s -> disconnected", __func__);
    mqtt_stat.connected = false;
    mqtt_stat.subscribed = false;
    SEMA_RELEASE();
}

static void mqtt_connect_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_e status)
{
    (void)client;
    (void)arg;
    LOG("%s -> status=%d", __func__, status);
    mqtt_stat.connected = (status == MQTT_CONNECT_ACCEPTED);
    s_async_err = mqtt_stat.connected ? 0 : (int)status;
    SEMA_RELEASE();
}

static void mqtt_request_cb(mqtt_client_t *client, void *arg, int err)
{
    (void)client;
    (void)arg;
    LOG("%s -> err=%d", __func__, err);
    s_async_err = err;
    SEMA_RELEASE();
}

static void mqtt_inpub_cb(mqtt_client_t *client, void *arg, int pkt_id,
                          const char *topic, const unsigned char *payload, unsigned short payload_len)
{
    (void)client;
    (void)arg;
    (void)pkt_id;
    mqtt_stat.received++;
    LOG("%s -> topic=%s len=%u payload=%s", __func__, topic, payload_len, payload);
    if (payload != NULL && payload_len > 0)
        cmd_procc(CMD_SRC_MQTT, payload, payload_len);
}

static void mqtt_disconnect_cb(mqtt_client_t *client, void *arg, int err)
{
    (void)client;
    (void)arg;
    LOG("%s -> err=%d", __func__, err);
    mqtt_stat.connected = false;
    mqtt_stat.subscribed = false;
    s_async_err = err;
    SEMA_RELEASE();
}

static int wait_async(int ret, const char *step)
{
    s_async_err = 0;

    if (ret == MQTTCLIENT_WOUNDBLOCK)
    {
        if (SEMA_WAIT() != QL_OSI_SUCCESS)
        {
            LOG("%s fail: sem wait", step);
            return -1;
        }

        if (s_async_err != 0)
        {
            LOG("%s fail: err=%d", step, s_async_err);
            return -1;
        }
        return 0;
    }

    if (ret != MQTTCLIENT_SUCCESS)
    {
        LOG("%s fail: ret=%d", step, ret);
        return -1;
    }

    return 0;
}

static void mqtt_session_stop(mqtt_client_t *cli)
{
    LOG("%s", __func__);
    if (cli == NULL)
        return;

    if (mqtt_stat.connected)
    {
        if (wait_async(ql_mqtt_disconnect(cli, mqtt_disconnect_cb, NULL), "disconnect") != 0)
            LOG("disconnect fail");
    }

    if (mqtt_stat.cli_inited)
    {
        ql_mqtt_client_deinit(cli);
        mqtt_stat.cli_inited = false;
        *cli = 0;
    }

    mqtt_stat.connected = false;
    mqtt_stat.subscribed = false;
    LOG("session stopped");
}

static int mqtt_session_start(mqtt_client_t *cli)
{
    struct mqtt_connect_client_info_t info = {0};
    uint16_t sim_cid = 0;
    int ret;

    LOG("%s", __func__);

    if (mqtt_stat.cli_inited)
        mqtt_session_stop(cli);

    snprintf(s_client_id, sizeof(s_client_id), "%s_%lu",
             MQTT_CLIENT_ID_BASE, (unsigned long)ql_rtos_get_system_tick());

    LOG("mqtt connecting %s id=%s", MQTT_BROKER_URL, s_client_id);
    info.keep_alive = 60;
    info.pkt_timeout = 5;
    info.retry_times = 3;
    info.clean_session = 1;
    info.client_id = s_client_id;
    info.client_user = "";
    info.client_pass = "";

    if (ql_bind_sim_and_profile(MQTT_SIM_ID, MQTT_PROFILE_IDX, &sim_cid) != QL_DATACALL_SUCCESS)
    {
        LOG("bind sim/profile fail");
        return -1;
    }
    LOG("bind ok sim_cid=%u", sim_cid);

    if (ql_mqtt_client_init(cli, sim_cid) != MQTTCLIENT_SUCCESS)
    {
        LOG("client init fail");
        return -1;
    }
    mqtt_stat.cli_inited = true;
    LOG("client init ok");

    ret = ql_mqtt_connect(cli, MQTT_BROKER_URL, mqtt_connect_cb, NULL, &info, mqtt_exception_cb);
    if (wait_async(ret, "connect") != 0 || !mqtt_is_connected())
    {
        LOG("connect fail connected=%d", mqtt_stat.connected);
        return -1;
    }
    LOG("connect ok");

    ql_mqtt_set_inpub_callback(cli, mqtt_inpub_cb, NULL);

    LOG("mqtt subscribing %s", MQTT_CMD_SUB_TOPIC);
    ret = ql_mqtt_sub_unsub(cli, MQTT_CMD_SUB_TOPIC, 1, mqtt_request_cb, NULL, 1);
    if (wait_async(ret, "subscribe") != 0)
        return -1;

    mqtt_stat.subscribed = true;
    LOG("session ok pub=%d sub=%s", mqtt_stat.published, MQTT_CMD_SUB_TOPIC);
    return 0;
}

static void mqtt_publish_loop(mqtt_client_t *cli)
{
    log_pkt_t logger_pkt = {0};
    log_pkt_t answer_pkt = {0};

    LOG("%s", __func__);

    while (mqtt_is_connected())
    {
        if (pkt_queue_recv(answer2mqtt_q, &answer_pkt, 0) == 0)
        {
            if (answer_pkt.len == 0)
                continue;

            LOG("publish answer %u bytes", answer_pkt.len);
            if (wait_async(ql_mqtt_publish(cli, MQTT_ANSWER_PUB_TOPIC,
                                           (const char *)answer_pkt.data, answer_pkt.len,
                                           0, 0, mqtt_request_cb, NULL), "publish answer") != 0)
            {
                LOG("publish answer fail len=%u", answer_pkt.len);
                break;
            }
            continue;
        }

        if (pkt_queue_recv(uart2mqtt_q, &logger_pkt, 1000) != 0)
            continue;

        if (logger_pkt.len == 0)
            continue;

        LOG("publish %u bytes", logger_pkt.len);
        if (wait_async(ql_mqtt_publish(cli, MQTT_LOG_PUB_TOPIC, (const char *)logger_pkt.data, logger_pkt.len,
                                       0, 0, mqtt_request_cb, NULL), "publish") != 0)
        {
            LOG("publish fail len=%u", logger_pkt.len);
            break;
        }

        mqtt_stat.published++;
        LOG("publish ok len=%u total_packets=%d", logger_pkt.len, mqtt_stat.published);
    }
}

static int wait_internet(void)
{
    int i;

    LOG("wait internet");
    for (i = 0; i < 120; i++)
    {
        sys_get();
        if (sys_internet_ready())
        {
            LOG("internet ready");
            return 0;
        }
        ql_rtos_task_sleep_s(1);
    }

    LOG("internet timeout");
    return -1;
}

static void mqtt_proc(void *arg)
{
    mqtt_client_t cli = 0;

    (void)arg;

    LOG("%s", __func__);
    LOG("=======================================");
    LOG("this window porccess the mqtt connection and publish the uart data to the mqtt broker\r\n");

    sys_set_mqtt_stat(&mqtt_stat);
    if (SEMA_CREATE() != QL_OSI_SUCCESS)
    {
        LOG("init fail: sem");
        goto exit;
    }
    LOG("init ok: sem");

    if (wait_internet() != 0)
        goto exit;

    while (1)
    {
        if (!sys_internet_ready())
        {
            if (sys_network_up() != 0)
            {
                LOG("internet not ready -> retry 5s");
                ql_rtos_task_sleep_s(5);
                continue;
            }
        }

        if (!mqtt_is_connected())
        {
            mqtt_session_stop(&cli);
            
            if (mqtt_session_start(&cli) != 0)
            {
                LOG("session start fail -> retry 5s");
                mqtt_session_stop(&cli);
                ql_rtos_task_sleep_s(5);
                continue;
            }
        }

        mqtt_publish_loop(&cli);

        if (!mqtt_is_connected() && mqtt_stat.cli_inited)
            mqtt_session_stop(&cli);
    }

exit:
    LOG("task exit");
    mqtt_session_stop(&cli);
    SEMA_DELETE();
    s_sem = NULL;
    ql_rtos_task_delete(s_task);
}

int mqtt_uart_post(const log_pkt_t *log_pkt)
{
    return pkt_queue_post(uart2mqtt_q, log_pkt);
}

int mqtt_answer_post(const ans_pkt_t *answer_pkt)
{
    return pkt_queue_post(answer2mqtt_q, answer_pkt);
}

int dani_mqtt_app_init(void)
{
    uart2mqtt_q = pkt_queue_init(sizeof(log_pkt_t), MQTT_UART_MAX_PKTS);
    if (uart2mqtt_q == NULL)
    {
        LOG("init fail: uart queue");
        return -1;
    }

    answer2mqtt_q = pkt_queue_init(sizeof(ans_pkt_t), MQTT_UART_MAX_PKTS);
    if (answer2mqtt_q == NULL)
    {
        LOG("init fail: answer queue");
        return -1;
    }

    if (ql_rtos_task_create(&s_task, 16 * 1024, APP_PRIORITY_ABOVE_NORMAL,
                           "dani_mqtt", mqtt_proc, NULL, 5) != QL_OSI_SUCCESS)
    {
        LOG("init fail: task");
        return -1;
    }

    return 0;
}
