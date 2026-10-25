#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ql_api_osi.h"
#include "ql_api_rtc.h"
#include "ql_pin_cfg.h"
#include "ql_sdmmc.h"
#include "ql_fs.h"
#include "dani_log.h"
#include "dani_sys.h"
#include "setting.h"
#include "pkt_queue.h"
#include "sd_proc.h"

#define DANI_LOG_FRAME_ID       Window14
#define LOG(msg, ...)           DANI_LOG(DANI_LOG_FRAME_ID, QL_LOG_LEVEL_INFO, "sd_proc", msg, ##__VA_ARGS__)

#define SD_TASK_STACK           (4096 * 3)
#define SD_TASK_PRIO            APP_PRIORITY_NORMAL
#define SD_BOOT_WAIT_S          15
#define SD_BOOT_MOUNT_TRIES     3
#define SD_BOOT_MOUNT_GAP_MS    1000
#define SD_QUEUE_DEPTH          8
#define SD_RECV_TIMEOUT_MS      500
#define SD_FILE_INTERVAL_MIN_MS 60000

typedef struct
{
    bool sd_exist;
    bool sd_enable;
    int sd_fd;
    char sd_path[SD_PROC_PATH_MAX];
    char sd_current_file_name[SD_PROC_FILENAME_MAX];
    int64_t sd_next_time_create_file;
} sd_card_state_t;

static ql_task_t s_task = NULL;
static pkt_queue_t *s_log_q = NULL;
static sd_card_state_t sd = {0};


bool sd_proc_is_mounted(void)
{
    return sd.sd_exist;
}

int sd_log_post(const log_pkt_t *log_pkt)
{
    if (log_pkt == NULL || s_log_q == NULL || !sd.sd_enable)
        return -1;

    if (pkt_queue_post(s_log_q, log_pkt) != 0)
    {
        LOG("queue full, drop %u bytes", log_pkt->len);
        return -1;
    }
    return 0;
}

static int64_t sd_file_interval_sec(void)
{
    int ms = get_logger_capture_time();

    if (ms < SD_FILE_INTERVAL_MIN_MS)
        ms = SD_FILE_INTERVAL_MIN_MS;
    return (int64_t)(ms / 1000);
}

static void sd_task_delete(void)
{
    if (sd.sd_fd >= 0)
    {
        ql_fclose(sd.sd_fd);
        sd.sd_fd = -1;
    }
    sd.sd_exist = false;
    sd.sd_current_file_name[0] = '\0';
    s_task = NULL;
    ql_rtos_task_delete(NULL);
}

/** Pin init + mount + SERIAL_LOG directory. @return 0 ok, -1 fail */
static int sd_init(void)
{
    ql_sdmmc_cfg_t cfg = {0};
    unsigned int i;

    LOG("pin init: wait %u s", SD_BOOT_WAIT_S);
    ql_rtos_task_sleep_s(SD_BOOT_WAIT_S);

    ql_pin_set_func(QL_PIN_SDMMC_CMD, QL_PIN_SDMMC_MODE_FUNC);
    ql_pin_set_func(QL_PIN_SDMMC_DATA_0, QL_PIN_SDMMC_MODE_FUNC);
    ql_pin_set_func(QL_PIN_SDMMC_DATA_1, QL_PIN_SDMMC_MODE_FUNC);
    ql_pin_set_func(QL_PIN_SDMMC_DATA_2, QL_PIN_SDMMC_MODE_FUNC);
    ql_pin_set_func(QL_PIN_SDMMC_DATA_3, QL_PIN_SDMMC_MODE_FUNC);
    ql_pin_set_func(QL_PIN_SDMMC_CLK, QL_PIN_SDMMC_MODE_FUNC);

    cfg.dev = QL_SDMMC_SD_CARD_ONLY;
    ql_sdmmc_set_dev_cfg(cfg);

    for (i = 0; i < SD_BOOT_MOUNT_TRIES; i++)
    {
        ql_errcode_sdmmc_e mret = ql_sdmmc_mount();

        if (mret == QL_SDMMC_SUCCESS)
        {
            sd.sd_exist = true;
            LOG("mount ok");
            break;
        }
        LOG("mount fail try %u err=%d", i + 1, (int)mret);
        sd.sd_exist = false;
        if (i + 1 < SD_BOOT_MOUNT_TRIES)
            ql_rtos_task_sleep_ms(SD_BOOT_MOUNT_GAP_MS);
    }

    if (!sd.sd_exist)
        return -1;

    {
        int err = ql_mkdir(sd.sd_path, 0);

        if (err < 0 && err != QL_DIR_DIR_ALREADY_EXIST)
        {
            LOG("mkdir fail %s err=%d", sd.sd_path, err);
            return -1;
        }
        LOG("log dir ok");
    }

    return 0;
}

/**
 * Open or rotate the active serial log file on SD.
 *
 * Builds the file name from RTC (yyyy-mm-dd___hh-mm.txt). Opens a new file when
 * none is open, when logger_capture_time has elapsed since the last open, or when
 * the minute bucket changes. Appends via ql_fopen "ab"; updates sd.sd_fd and
 * sd.sd_next_time_create_file on success.
 */
static void sd_handle_file(int64_t now_sec, const ql_rtc_time_t *rtc, int64_t interval_sec)
{
    char file_path[SD_PROC_PATH_MAX + SD_PROC_FILENAME_MAX];
    char new_name[SD_PROC_FILENAME_MAX];
    int need_new_file = 0;

    if (rtc == NULL)
        return;

    if (sd.sd_fd < 0 || now_sec >= sd.sd_next_time_create_file)
        need_new_file = 1;

    snprintf(new_name, sizeof(new_name), "%04d-%02d-%02d___%02d-%02d.txt",
             rtc->tm_year, rtc->tm_mon, rtc->tm_mday, rtc->tm_hour, rtc->tm_min);

    if (sd.sd_fd >= 0 && strcmp(new_name, sd.sd_current_file_name) != 0)
        need_new_file = 1;

    if (!need_new_file)
        return;

    if (sd.sd_fd >= 0)
    {
        ql_fclose(sd.sd_fd);
        sd.sd_fd = -1;
    }

    snprintf(file_path, sizeof(file_path), "%s/%s", sd.sd_path, new_name);
    sd.sd_fd = ql_fopen(file_path, "ab");
    if (sd.sd_fd < 0)
    {
        LOG("fopen fail %s err=%d", file_path, sd.sd_fd);
        sd.sd_fd = -1;
        return;
    }

    strncpy(sd.sd_current_file_name, new_name, sizeof(sd.sd_current_file_name) - 1);
    sd.sd_next_time_create_file = now_sec + interval_sec;
    LOG("open log %s next_rotate=%lld", file_path, (long long)sd.sd_next_time_create_file);
}

static void sd_proc_task(void *param)
{
    ql_rtc_time_t rtc;
    int64_t now_sec;
    int64_t interval_sec;

    (void)param;

    sd.sd_exist = false;
    sd.sd_enable = is_sd_card_enabled();
    sd.sd_fd = -1;
    sd.sd_path[0] = '\0';
    sd.sd_current_file_name[0] = '\0';
    sd.sd_next_time_create_file = 0;

    LOG("%s", __func__);
    LOG("=========================================");
    LOG("this window porccess sd card serial logging\r\n");

    strncpy(sd.sd_path, "SD:/SERIAL_LOG", sizeof(sd.sd_path) - 1);

    if (!sd.sd_enable)
    {
        LOG("sd_enable=0 -> task delete");
        sd_task_delete();
        return;
    }

    LOG("sd_enable=1 interval=%lld sec path=%s", (long long)sd_file_interval_sec(), sd.sd_path);

    if (sd_init() != 0)
    {
        LOG("no SD at boot -> task delete");
        sd_task_delete();
        return;
    }

    interval_sec = sd_file_interval_sec();
    sd.sd_fd = -1;
    sd.sd_next_time_create_file = 0;

    LOG("logging loop: queue + file rotate every %lld sec", (long long)interval_sec);

    while (1)
    {
        log_pkt_t pkt = {0};

        if (ql_rtc_get_time(&rtc) != QL_RTC_SUCCESS)
        {
            LOG("rtc fail");
            ql_rtos_task_sleep_ms(SD_RECV_TIMEOUT_MS);
            continue;
        }

        if (rtc_to_unix(&now_sec, &rtc) != 0)
        {
            ql_rtos_task_sleep_ms(SD_RECV_TIMEOUT_MS);
            continue;
        }

        sd_handle_file(now_sec, &rtc, interval_sec);

        if (pkt_queue_recv(s_log_q, &pkt, SD_RECV_TIMEOUT_MS) != 0)
            continue;

        if (pkt.len == 0 || sd.sd_fd < 0)
            continue;

        {
            int w = ql_fwrite(pkt.data, pkt.len, 1, sd.sd_fd);

            if (w < 0)
                LOG("fwrite fail len=%u err=%d", pkt.len, w);
            else
                LOG("wrote %u bytes to %s", pkt.len, sd.sd_current_file_name);
        }
    }
}

void sd_proc_init(void)
{
    if (s_log_q == NULL)
    {
        s_log_q = pkt_queue_init(sizeof(log_pkt_t), SD_QUEUE_DEPTH);
        if (s_log_q == NULL)
        {
            LOG("init fail: queue");
            return;
        }
    }

    if (s_task != NULL)
    {
        LOG("init skipped (running)");
        return;
    }

    if (ql_rtos_task_create(&s_task, SD_TASK_STACK, SD_TASK_PRIO, "sd_proc",
                            sd_proc_task, NULL, 1) != QL_OSI_SUCCESS)
    {
        LOG("init fail: task");
        s_task = NULL;
        return;
    }

    LOG("init ok");
}
