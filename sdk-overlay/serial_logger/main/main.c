#include "ql_app_feature_config.h"
#include "dani_log.h"
#include "main.h"
#include "setting.h"
#include "mqtt_proc.h"
#include "sys_proc.h"
#include "input_log_proc.h"
#include "sms_proc.h"
#include "sd_proc.h"
#include "io_proc.h"

#define DANI_LOG_FRAME_ID  Window11
#define LOG(msg, ...)      DANI_LOG(DANI_LOG_FRAME_ID, QL_LOG_LEVEL_INFO, "main", msg, ##__VA_ARGS__)

int main(void)
{
    if (dani_log_init() != 0)
        return -1;

    LOG("start");

    if (!setting_load())
        LOG("settings load failed, defaults applied");

    sd_proc_init();

    setting_print(NULL);

    dani_mqtt_app_init();
    io_proc_init();
    sys_proc_init();
    dani_input_log_proc_app_init();
    sms_proc_init();

    return 0;
}
