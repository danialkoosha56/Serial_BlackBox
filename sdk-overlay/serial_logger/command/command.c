#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ql_api_osi.h"
#include "ql_power.h"
#include "dani_log.h"
#include "dani_sys.h"
#include "command.h"
#include "setting.h"
#include "mqtt_proc.h"
#include "sms_proc.h"
#include "io_proc.h"

#define DANI_LOG_FRAME_ID  Window11
#define LOG(msg, ...)      DANI_LOG(DANI_LOG_FRAME_ID, QL_LOG_LEVEL_INFO, "command", msg, ##__VA_ARGS__)


static char s_line[CMD_MAX_LEN];



bool cmd_ans(cmd_src_t cmd_src,char *msg);



static const char cmd_help_text[] =
    "\r\n\r\n=========COMMAND EXAMPLES:=========\r\n"
    "help\r\n"
    "status\r\n"
    "reboot\r\n"
    "echo hello\r\n"
    "sdcard enable/disable\r\n"
    "mqtt enable/disable\r\n"
    "brokerurl test.mosquitto.org\r\n"
    "brokerport 1883\r\n"
    "brokeruser myuser\r\n"
    "brokerpass secret\r\n"
    "brokertopic my/test/uart/data\r\n"
    "uartbaud 115200\r\n"
    "capturesize 2048\r\n"
    "capturetime 10000\r\n"
    "output1 high/low\r\n"
    "setpin high|low\r\n"
    "readinput\r\n"
    "readadc\r\n"
    "cmd2device enable\r\n"
    "===================================\r\n";

static bool cmd_help_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    (void)str;

    cmd_ans(cmd_src, (char *)cmd_help_text);

    return 0;
}

static bool cmd_status_cb(cmd_src_t cmd_src, char *str)
{
    char msg[256];
    int n;

    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    (void)str;

    sys_get();

    n = snprintf(msg, sizeof(msg),
                 "sim=%s iccid=%s\ncsq=%u data=%s ip=%s\ninternet=%s uptime=%lu s",
                 sys_sim_status_str(sys_info.sim_status),
                 sys_info.iccid[0] ? sys_info.iccid : "-",
                 sys_info.csq,
                 sys_reg_state_str(sys_info.data_reg),
                 sys_info.ip[0] ? sys_info.ip : "-",
                 sys_internet_ready() ? "ready" : "down",
                 sys_info.lifetime);

    if (n > 0 && (size_t)n < sizeof(msg))
    {
        if (sys_info.mqtt != NULL)
        {
            snprintf(msg + n, sizeof(msg) - (size_t)n,
                     "\nmqtt conn=%d pub=%d rx=%d",
                     sys_info.mqtt->connected,
                     sys_info.mqtt->published,
                     sys_info.mqtt->received);
        }
        else
        {
            snprintf(msg + n, sizeof(msg) - (size_t)n, "\nmqtt=n/a");
        }
    }

    cmd_ans(cmd_src, msg);
    return 0;
}

static bool cmd_reboot_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    (void)str;

    cmd_ans(cmd_src, "rebooting");
    ql_rtos_task_sleep_ms(200);
    ql_power_reset(RESET_NORMAL);
    return 0;
}

static bool cmd_echo_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);

    if (str == NULL || *str == '\0')
    {
        cmd_ans(cmd_src, "usage: echo <text>");
        return false;
    }

    cmd_ans(cmd_src, str);
    return 0;
}

static bool cmd_sd_card_enable_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    bool val;
    char log[128] = {0};

    if (strstr(str, "enable"))
        val = true;
    else if (strstr(str, "disable"))
        val = false;
    else
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    if (setting_update_sd_card_enabled(val, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_mqtt_enable_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    bool val;
    char log[128] = {0};

    if (strstr(str, "enable"))
        val = true;
    else if (strstr(str, "disable"))
        val = false;
    else
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    if (setting_update_mqtt_enabled(val, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_broker_url_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    char log[128] = {0};

    if (str == NULL || *str == '\0')
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    if (setting_update_broker_url(str, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_broker_port_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    char log[128] = {0};
    int port;

    if (str == NULL || *str == '\0')
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    port = atoi(str);
    if (setting_update_broker_port(port, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_broker_username_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    char log[128] = {0};

    if (str == NULL || *str == '\0')
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    if (setting_update_broker_username(str, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_broker_password_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    char log[128] = {0};

    if (str == NULL || *str == '\0')
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    if (setting_update_broker_password(str, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_broker_topic_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    char log[128] = {0};

    if (str == NULL || *str == '\0')
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    if (setting_update_broker_topic(str, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_uart_baudrate_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    char log[128] = {0};
    int baud;

    if (str == NULL || *str == '\0')
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    baud = atoi(str);
    if (setting_update_uart_baudrate(baud, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_logger_capture_size_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    char log[128] = {0};
    int size;

    if (str == NULL || *str == '\0')
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    size = atoi(str);
    if (setting_update_logger_capture_size(size, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_logger_capture_time_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    char log[128] = {0};
    int time_ms;

    if (str == NULL || *str == '\0')
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    time_ms = atoi(str);
    if (setting_update_logger_capture_time(time_ms, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_setpin_cb(cmd_src_t cmd_src, char *str)
{
    char log[IO_PROC_LOG_SIZE] = {0};

    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    (void)str;

    if (str != NULL && strstr(str, "high"))
    {
        if (io_proc_set_output(true, log, sizeof(log)) == 0)
        {
            cmd_ans(cmd_src, log);
            return true;
        }
    }
    else if (str != NULL && strstr(str, "low"))
    {
        if (io_proc_set_output(false, log, sizeof(log)) == 0)
        {
            cmd_ans(cmd_src, log);
            return true;
        }
    }
    else
    {
        cmd_ans(cmd_src, "usage: setpin high|low");
        return false;
    }

    cmd_ans(cmd_src, log[0] ? log : "setpin fail");
    return false;
}

static bool cmd_readinput_cb(cmd_src_t cmd_src, char *str)
{
    char log[IO_PROC_LOG_SIZE] = {0};
    int level = 0;

    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    (void)str;

    if (io_proc_read_input(&level, log, sizeof(log)) == 0)
    {
        cmd_ans(cmd_src, log);
        return true;
    }

    cmd_ans(cmd_src, log[0] ? log : "readinput fail");
    return false;
}

static bool cmd_readadc_cb(cmd_src_t cmd_src, char *str)
{
    char log[IO_PROC_LOG_SIZE] = {0};
    int mv = 0;

    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    (void)str;

    if (io_proc_read_adc(&mv, log, sizeof(log)) == 0)
    {
        cmd_ans(cmd_src, log);
        return true;
    }

    cmd_ans(cmd_src, log[0] ? log : "readadc fail");
    return false;
}

static bool cmd_logger_output1_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    bool val;
    char log[128] = {0};

    if (strstr(str, "enable"))
        val = true;
    else if (strstr(str, "disable"))
        val = false;
    else
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    if (setting_update_logger_output1_state(val, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_cmd_to_device_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    bool val;
    char log[128] = {0};

    if (strstr(str, "enable"))
        val = true;
    else if (strstr(str, "disable"))
        val = false;
    else
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    if (setting_update_cmd_to_device_permision(val, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

static bool cmd_phone_number_cb(cmd_src_t cmd_src, char *str)
{
    LOG("FUNCTION EXECUTE: %s", __FUNCTION__);
    char log[128] = {0};

    if (str == NULL || *str == '\0')
    {
        cmd_ans(cmd_src, "bad input!!!");
        return false;
    }

    if (setting_update_phone_number(str, log))
    {
        cmd_ans(cmd_src, log);
        return 1;
    }
    cmd_ans(cmd_src, log);
    return 0;
}

// PUT IT IN .H AND DECLEAR EACH FUNCTION IN SUTABLE FILE, NOT ALL HERE
const cmd_entry_t command_list[] =
{
    { "help",   cmd_help_cb },
    { "status", cmd_status_cb },
    { "reboot", cmd_reboot_cb },
    { "echo",   cmd_echo_cb },

    { "sdcard",      cmd_sd_card_enable_cb },
    { "mqtt",        cmd_mqtt_enable_cb },
    { "brokerurl",   cmd_broker_url_cb },
    { "brokerport",  cmd_broker_port_cb },
    { "brokeruser",  cmd_broker_username_cb },
    { "brokerpass",  cmd_broker_password_cb },
    { "brokertopic", cmd_broker_topic_cb },
    { "uartbaud",    cmd_uart_baudrate_cb },
    { "capturesize", cmd_logger_capture_size_cb },
    { "capturetime", cmd_logger_capture_time_cb },
    { "output1",       cmd_logger_output1_cb },
    { "setpin",        cmd_setpin_cb },
    { "readinput",     cmd_readinput_cb },
    { "readadc",       cmd_readadc_cb },
    { "cmd2device",  cmd_cmd_to_device_cb },

    {"phonenumber", cmd_phone_number_cb },
};


// remove leading and trailing whitespace characters from a string, and remove any trailing '\r' or '\n' characters.
static void cmd_to_lower(char *s)
{
    unsigned char *p = (unsigned char *)s;

    if (p == NULL)
        return;

    for (; *p != '\0'; p++)
        *p = (unsigned char)tolower(*p);
}

static void trim_line(char *s)
{
    char *start;
    char *end;
    size_t len;

    if (s == NULL)
        return;

    start = s;
    while (*start != '\0' && isspace((unsigned char)*start))
        start++;

    if (start != s)
        memmove(s, start, strlen(start) + 1);

    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
    {
        s[len - 1] = '\0';
        len--;
    }

    end = strchr(s, '\r');
    if (end != NULL)
        *end = '\0';

    end = strchr(s, '\n');
    if (end != NULL)
        *end = '\0';
}

static char *cmd_args(const char *name, char *line)
{
    size_t n = strlen(name);

    line += n;
    while (*line != '\0' && isspace((unsigned char)*line))
        line++;

    return line;
}

static void cmd_parsser(cmd_src_t cmd_src, char *line)
{
    unsigned int i;
    unsigned int count = sizeof(command_list) / sizeof(command_list[0]);

    if (line == NULL || *line == '\0')
        return;

    for (i = 0; i < count; i++)
    {
        if (strstr(line, command_list[i].cmd_name))
        {
            command_list[i].func_cb(cmd_src, cmd_args(command_list[i].cmd_name, line));
            return;
        }
    }

    LOG("unknown command: %s", line);
    cmd_help_cb(cmd_src, "");
}

bool cmd_ans(cmd_src_t cmd_src,char *msg)
{
    const char *command_source_str[] = { "UART", "MQTT", "SMS" };
    LOG("command source= %s", command_source_str[cmd_src]);
    LOG("answer msg: %s", msg);

    switch (cmd_src)
    {
        case CMD_SRC_UART:
            break;

        case CMD_SRC_MQTT:
        {
            ans_pkt_t answer_pkt = {0};
            size_t len;

            if (msg == NULL)
                return false;

            len = strlen(msg);
            if (len >= UART_PKT_DATA_SIZE)
                len = UART_PKT_DATA_SIZE - 1;

            answer_pkt.len = (unsigned int)len;
            memcpy(answer_pkt.data, msg, len);
            answer_pkt.data[len] = '\0';

            if (mqtt_answer_post(&answer_pkt) != 0)
                return false;
            break;
        }

        case CMD_SRC_SMS:
            if (msg == NULL)
                return false;
            if (sms_send_text(msg) != 0)
                return false;
            break;
            
        default:
            LOG("unknown command source: %d", cmd_src);
            return false;
    }
    return true;

}

void cmd_procc(cmd_src_t cmd_src, const unsigned char *payload, unsigned int payload_len)
{
    unsigned int copy_len;

    if (payload == NULL || payload_len == 0)
        return;

    copy_len = payload_len;
    if (copy_len >= CMD_MAX_LEN)
        copy_len = CMD_MAX_LEN - 1;

    memcpy(s_line, payload, copy_len);
    s_line[copy_len] = '\0';
    trim_line(s_line);
    cmd_to_lower(s_line);

    if (s_line[0] == '\0')
        return;

    LOG("\r\nrun: %s", s_line);
    cmd_parsser(cmd_src, s_line);
}
