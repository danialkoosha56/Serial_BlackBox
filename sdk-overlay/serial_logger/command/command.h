#ifndef COMMAND_H
#define COMMAND_H

#define CMD_MAX_LEN  128

typedef enum
{
    CMD_SRC_UART,
    CMD_SRC_MQTT,
    CMD_SRC_SMS
} cmd_src_t;

typedef struct
{
    char cmd_name[20];
    bool (*func_cb)(cmd_src_t cmd_src, char *str);
} cmd_entry_t;

void cmd_procc(cmd_src_t cmd_src, const unsigned char *payload, unsigned int payload_len);

#endif /* COMMAND_H */
