#ifndef SETTING_H
#define SETTING_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool sd_card_enabled;
    bool mqtt_enabled;
    char broker_url[100];
    int broker_port;
    char broker_username[100];
    char broker_password[100];
    char broker_topic[100];
    int uart_baudrate;
    int logger_capture_size;
    int logger_capture_time;
    bool logger_output1_state;
    bool cmd_to_device_permision;
    char phone_number[20];
    int CRC32;
} settings_t;

/** Factory default values (used by load_default_setting()). */
#define SETTING_SD_CARD_ENABLED_DEFAULT         false
#define SETTING_MQTT_ENABLED_DEFAULT            true
#define SETTING_BROKER_URL_DEFAULT              "test.mosquitto.org"
#define SETTING_BROKER_PORT_DEFAULT             1883
#define SETTING_BROKER_USERNAME_DEFAULT         ""
#define SETTING_BROKER_PASSWORD_DEFAULT         ""
#define SETTING_BROKER_TOPIC_DEFAULT            "my/test/uart/data"
#define SETTING_UART_BAUDRATE_DEFAULT           115200
#define SETTING_LOGGER_CAPTURE_SIZE_DEFAULT     2048
#define SETTING_LOGGER_CAPTURE_TIME_DEFAULT     10000
#define SETTING_LOGGER_output1_STATE_DEFAULT      false
#define SETTING_CMD_TO_DEVICE_PERMISION_DEFAULT false
#define SETTING_PHONE_NUMBER_DEFAULT            ""
#define SETTING_CRC32_DEFAULT                   0

/** Minimum buffer size for @p log in setting_update_* functions. */
#define SETTING_UPDATE_LOG_SIZE                 128

/** Global settings instance. Populated by setting_load() / setting_load_default(). */
extern settings_t settings;

#define is_sd_card_enabled()           (settings.sd_card_enabled)
#define is_mqtt_enabled()              (settings.mqtt_enabled)
#define is_cmd_to_device_permitted()   (settings.cmd_to_device_permision)
#define get_broker_url()                (settings.broker_url)
#define get_broker_port()               (settings.broker_port)
#define get_broker_username()           (settings.broker_username)
#define get_broker_password()           (settings.broker_password)
#define get_broker_topic()              (settings.broker_topic)
#define get_uart_baudrate()             (settings.uart_baudrate)
#define get_logger_capture_size()       (settings.logger_capture_size)
#define get_logger_capture_time()       (settings.logger_capture_time)
#define get_logger_output1_state()        (settings.logger_output1_state)
#define get_phone_number()              (settings.phone_number)
#define get_crc32()                     (settings.CRC32)

/** Fill global @c settings with factory/default values. */
void setting_load_default(void);

/** Load global @c settings from persistent storage. @return true on success. */
bool setting_load(void);

/** Save global @c settings to persistent storage. @return true on success. */
bool setting_update(void);
/** @param log Result message buffer (min SETTING_UPDATE_LOG_SIZE bytes), or NULL. */
bool setting_update_sd_card_enabled(bool enabled, char *log);
bool setting_update_mqtt_enabled(bool enabled, char *log);
bool setting_update_broker_url(const char *url, char *log);
bool setting_update_broker_port(int port, char *log);
bool setting_update_broker_username(const char *username, char *log);
bool setting_update_broker_password(const char *password, char *log);
bool setting_update_broker_topic(const char *topic, char *log);
bool setting_update_uart_baudrate(int baudrate, char *log);
bool setting_update_logger_capture_size(int size, char *log);
bool setting_update_logger_capture_time(int time_ms, char *log);
bool setting_update_logger_output1_state(bool state, char *log);
bool setting_update_cmd_to_device_permision(bool permitted, char *log);
bool setting_update_phone_number(const char *phone, char *log);

/**
 * @brief Log current global settings; copy text into @p set if not NULL.
 */
void setting_print(char *set);

#endif /* SETTING_H */
