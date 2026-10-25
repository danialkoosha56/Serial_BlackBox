#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "calclib/crc32.h"
#include "dani_log.h"
#include "ql_fs.h"
#include "setting.h"

#define DANI_LOG_FRAME_ID  Window11
#define LOG(msg, ...)      DANI_LOG(DANI_LOG_FRAME_ID, QL_LOG_LEVEL_INFO, "setting", msg, ##__VA_ARGS__)

#define SETTING_FILE_PATH       "UFS:/setting.txt"
#define SETTING_PRINT_BUF_SIZE  512
#define SETTING_BROKER_PORT_MIN 1
#define SETTING_BROKER_PORT_MAX 65535
#define SETTING_LOGGER_CAPTURE_SIZE_MIN 1
#define SETTING_LOGGER_CAPTURE_SIZE_MAX 2048
#define SETTING_LOGGER_CAPTURE_TIME_MIN 1

settings_t settings;

static void setting_str_cpy(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0)
        return;

    strncpy(dst, src != NULL ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void setting_log_write(char *log, const char *fmt, ...)
{
    va_list ap;

    if (log == NULL)
        return;

    va_start(ap, fmt);
    vsnprintf(log, SETTING_UPDATE_LOG_SIZE, fmt, ap);
    va_end(ap);
}


static bool setting_validation_crc(const settings_t *s)
{
    int now_crc = s->CRC32;
    int shouldbe_crc = (int)crc32Calc(s, offsetof(settings_t, CRC32));
    return  now_crc == shouldbe_crc;
}

static void setting_calc_crc(settings_t *s)
{
    if (s != NULL)
        s->CRC32 = (int)crc32Calc(s, offsetof(settings_t, CRC32));
}

static bool setting_write_file(const settings_t *src)
{
    QFILE fd;
    int err;

    if (src == NULL)
        return false;

    fd = ql_fopen(SETTING_FILE_PATH, "wb+");
    if (fd < 0)
    {
        LOG("failed to open %s err: %d", SETTING_FILE_PATH, fd);
        return false;
    }

    err = ql_fwrite((void *)src, sizeof(*src), 1, fd);
    ql_fclose(fd);

    if (err <= 0)
    {
        LOG("failed to write %s err: %d", SETTING_FILE_PATH, err);
        return false;
    }

    LOG("%s saved (%d bytes)", SETTING_FILE_PATH, (int)sizeof(*src));
    return true;
}

static bool setting_create_file(void)
{
    setting_calc_crc(&settings);
    return setting_write_file(&settings);
}

void setting_load_default(void)
{
    memset(&settings, 0, sizeof(settings));
    settings.sd_card_enabled = SETTING_SD_CARD_ENABLED_DEFAULT;
    settings.mqtt_enabled = SETTING_MQTT_ENABLED_DEFAULT;
    setting_str_cpy(settings.broker_url, sizeof(settings.broker_url), SETTING_BROKER_URL_DEFAULT);
    settings.broker_port = SETTING_BROKER_PORT_DEFAULT;
    setting_str_cpy(settings.broker_username, sizeof(settings.broker_username), SETTING_BROKER_USERNAME_DEFAULT);
    setting_str_cpy(settings.broker_password, sizeof(settings.broker_password), SETTING_BROKER_PASSWORD_DEFAULT);
    setting_str_cpy(settings.broker_topic, sizeof(settings.broker_topic), SETTING_BROKER_TOPIC_DEFAULT);
    settings.uart_baudrate = SETTING_UART_BAUDRATE_DEFAULT;
    settings.logger_capture_size = SETTING_LOGGER_CAPTURE_SIZE_DEFAULT;
    settings.logger_capture_time = SETTING_LOGGER_CAPTURE_TIME_DEFAULT;
    settings.logger_output1_state = SETTING_LOGGER_output1_STATE_DEFAULT;
    settings.cmd_to_device_permision = SETTING_CMD_TO_DEVICE_PERMISION_DEFAULT;
    setting_str_cpy(settings.phone_number, sizeof(settings.phone_number), SETTING_PHONE_NUMBER_DEFAULT);
    setting_calc_crc(&settings);
}

bool setting_load(void)
{
    settings_t loaded = {0};
    QFILE fd;
    int err;

    if (ql_file_exist(SETTING_FILE_PATH) != QL_FILE_OK)
    {
        LOG("file not exist, create with default settings");
        setting_load_default();
        return setting_create_file();
    }

    fd = ql_fopen(SETTING_FILE_PATH, "rb");
    if (fd < 0)
    {
        LOG("failed to open %s err: %d", SETTING_FILE_PATH, fd);
        setting_load_default();
        return false;
    }

    err = ql_fread(&loaded, sizeof(loaded), 1, fd);
    ql_fclose(fd);

    if (err <= 0 || (size_t)err < sizeof(loaded))
    {
        LOG("failed to read %s err: %d", SETTING_FILE_PATH, err);
        setting_load_default();
        return false;
    }

    if (!setting_validation_crc(&loaded))
    {
        LOG("CRC mismatch, using defaults");
        setting_load_default();
        setting_create_file();
        return false;
    }

    settings = loaded;
    return true;
}

bool setting_update(void)
{
    setting_calc_crc(&settings);
    if (!setting_write_file(&settings))
    {
        LOG("setting_update: save FAILED");
        return false;
    }

    LOG("setting_update: all fields saved OK");
    return true;
}

static bool _str_ok(const char *s, bool required)
{
    if (s == NULL)
        return false;
    if (required && s[0] == '\0')
        return false;
    return true;
}

static bool _str_fitsk(const char *s, size_t field_size)
{
    return s != NULL && strlen(s) < field_size;
}

static bool _uart_baudrate_ok(int baudrate)
{
    static const int allowed[] = {
        1200, 2400, 4800, 9600, 10400, 14400, 19200, 28800, 33600, 38400,
        57600, 115200, 187500, 230400, 460800, 921600, 1000000, 1843200,
        2000000, 2100000, 3686400, 4000000, 4468750,
    };
    size_t i;

    for (i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++)
    {
        if (baudrate == allowed[i])
            return true;
    }
    return false;
}

static bool _apply_bool(const char *name, bool *field, bool value, char *log)
{
    settings_t backup = settings;

    *field = value;
    setting_calc_crc(&settings);
    if (!setting_write_file(&settings))
    {
        settings = backup;
        setting_log_write(log, "%s update FAILED, reverted (new=%d)", name, value);
        return false;
    }

    setting_log_write(log, "%s updated OK (new=%d)", name, value);
    return true;
}

static bool _apply_str(const char *name, char *field, size_t field_size,
                              const char *value, bool required, char *log)
{
    settings_t backup = settings;

    if (!_str_ok(value, required))
    {
        setting_log_write(log, "%s invalid input (null%s)", name, required ? " or empty" : "");
        return false;
    }
    if (!_str_fitsk(value, field_size))
    {
        setting_log_write(log, "%s too long (max %u chars)", name, (unsigned)(field_size - 1));
        return false;
    }

    setting_str_cpy(field, field_size, value);
    setting_calc_crc(&settings);
    if (!setting_write_file(&settings))
    {
        settings = backup;
        setting_log_write(log, "%s update FAILED, reverted (new=%s)", name, value);
        return false;
    }

    setting_log_write(log, "%s updated OK (new=%s)", name, value);
    return true;
}

static bool _apply_int(const char *name, int *field, int value,
                              int min_val, int max_val, char *log)
{
    settings_t backup = settings;

    if (value < min_val || value > max_val)
    {
        setting_log_write(log, "%s invalid input: %d (allowed %d..%d)",
                          name, value, min_val, max_val);
        return false;
    }

    *field = value;
    setting_calc_crc(&settings);
    if (!setting_write_file(&settings))
    {
        settings = backup;
        setting_log_write(log, "%s update FAILED, reverted (new=%d)", name, value);
        return false;
    }

    setting_log_write(log, "%s updated OK (new=%d)", name, value);
    return true;
}

bool setting_update_sd_card_enabled(bool enabled, char *log)
{
    return _apply_bool("sd_card_enabled", &settings.sd_card_enabled, enabled, log);
}

bool setting_update_mqtt_enabled(bool enabled, char *log)
{
    return _apply_bool("mqtt_enabled", &settings.mqtt_enabled, enabled, log);
}

bool setting_update_broker_url(const char *url, char *log)
{
    return _apply_str("broker_url", settings.broker_url,
                             sizeof(settings.broker_url), url, true, log);
}

bool setting_update_broker_port(int port, char *log)
{
    return _apply_int("broker_port", &settings.broker_port, port,
                             SETTING_BROKER_PORT_MIN, SETTING_BROKER_PORT_MAX, log);
}

bool setting_update_broker_username(const char *username, char *log)
{
    return _apply_str("broker_username", settings.broker_username,
                             sizeof(settings.broker_username), username, false, log);
}

bool setting_update_broker_password(const char *password, char *log)
{
    return _apply_str("broker_password", settings.broker_password,
                             sizeof(settings.broker_password), password, false, log);
}

bool setting_update_broker_topic(const char *topic, char *log)
{
    return _apply_str("broker_topic", settings.broker_topic,
                             sizeof(settings.broker_topic), topic, true, log);
}

bool setting_update_uart_baudrate(int baudrate, char *log)
{
    settings_t backup = settings;

    if (!_uart_baudrate_ok(baudrate))
    {
        setting_log_write(log, "uart_baudrate invalid input: %d", baudrate);
        return false;
    }

    settings.uart_baudrate = baudrate;
    setting_calc_crc(&settings);
    if (!setting_write_file(&settings))
    {
        settings = backup;
        setting_log_write(log, "uart_baudrate update FAILED, reverted (new=%d)", baudrate);
        return false;
    }

    setting_log_write(log, "uart_baudrate updated OK (new=%d)", baudrate);
    return true;
}

bool setting_update_logger_capture_size(int size, char *log)
{
    return _apply_int("logger_capture_size", &settings.logger_capture_size, size,
                             SETTING_LOGGER_CAPTURE_SIZE_MIN,
                             SETTING_LOGGER_CAPTURE_SIZE_MAX, log);
}

bool setting_update_logger_capture_time(int time_ms, char *log)
{
    return _apply_int("logger_capture_time", &settings.logger_capture_time, time_ms,
                             SETTING_LOGGER_CAPTURE_TIME_MIN, INT32_MAX, log);
}

bool setting_update_logger_output1_state(bool state, char *log)
{
    return _apply_bool("logger_output1_state", &settings.logger_output1_state, state, log);
}

bool setting_update_cmd_to_device_permision(bool permitted, char *log)
{
    return _apply_bool("cmd_to_device_permision",
                              &settings.cmd_to_device_permision, permitted, log);
}

bool setting_update_phone_number(const char *phone, char *log)
{
    return _apply_str("phone_number", settings.phone_number,
                             sizeof(settings.phone_number), phone, true, log);
}

void setting_print(char *set)
{
    static char msg[SETTING_PRINT_BUF_SIZE];
    char *p = msg;
    int n;

    n = sprintf(p, "sd_card_enabled: %d\n", settings.sd_card_enabled);
    p += n;
    n = sprintf(p, "mqtt_enabled: %d\n", settings.mqtt_enabled);
    p += n;
    n = sprintf(p, "broker_url: %s\n", settings.broker_url);
    p += n;
    n = sprintf(p, "broker_port: %d\n", settings.broker_port);
    p += n;
    n = sprintf(p, "broker_username: %s\n", settings.broker_username);
    p += n;
    n = sprintf(p, "broker_password: %s\n", settings.broker_password);
    p += n;
    n = sprintf(p, "broker_topic: %s\n", settings.broker_topic);
    p += n;
    n = sprintf(p, "uart_baudrate: %d\n", settings.uart_baudrate);
    p += n;
    n = sprintf(p, "logger_capture_size: %d\n", settings.logger_capture_size);
    p += n;
    n = sprintf(p, "logger_capture_time: %d\n", settings.logger_capture_time);
    p += n;
    n = sprintf(p, "logger_output1_state: %d\n", settings.logger_output1_state);
    p += n;
    n = sprintf(p, "cmd_to_device_permision: %d\n", settings.cmd_to_device_permision);
    p += n;
    n = sprintf(p, "phone_number: %s\n", settings.phone_number);
    p += n;
    sprintf(p, "CRC32: %d", settings.CRC32);

    LOG("\r\n\r\n======================================");
    LOG("======================================");
    LOG("======================================");
    LOG("============DEVICE SETTINGS:============\r\n%s", msg);
    LOG("======================================\r\n\r\n");
    if (set != NULL)
        strcpy(set, msg);
}

