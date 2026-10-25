#ifndef SD_PROC_H
#define SD_PROC_H

#include <stdbool.h>
#include <stdint.h>

#include "mqtt_proc.h"

#define SD_PROC_PATH_MAX        64
#define SD_PROC_FILENAME_MAX    40

void sd_proc_init(void);

/** Non-blocking post from serial log task (same packet as MQTT). */
int sd_log_post(const log_pkt_t *log_pkt);

bool sd_proc_is_mounted(void);


#endif /* SD_PROC_H */
