#ifndef SYS_H
#define SYS_H

#include <stdint.h>
#include <stdbool.h>
#include "ql_api_nw.h"
#include "ql_api_sim.h"
#include "ql_api_rtc.h"

#define SYS_SIM_ID              0
#define SYS_PROFILE_IDX_DEFAULT 1
#define SYS_APN_DEFAULT         "uninet"
#define SYS_ICCID_LEN           32
#define SYS_IMSI_LEN            32
#define SYS_OPER_LEN            64
#define SYS_MCC_LEN             8
#define SYS_MNC_LEN             8
#define SYS_IP_LEN              16

/**
 * @brief Live MQTT session stats (owned by mqtt_proc, linked into sys_info).
 */
typedef struct
{
    bool connected;
    bool subscribed;
    bool cli_inited;
    int published;
    int received;
} mqtt_stat_t;

/**
 * @brief Snapshot of SIM, network, PDP/IP and clock status.
 *
 * Updated by sys_get(). lifetime is uptime in seconds since bootstrap time sync.
 * internet_ready is 1 when PDP is active and IPv4 is available.
 * mqtt points to live mqtt_stat_t (NULL if mqtt not registered).
 */
typedef struct
{
    ql_sim_status_e sim_status;
    char iccid[SYS_ICCID_LEN];
    char imsi[SYS_IMSI_LEN];

    ql_nw_reg_state_e voice_reg;
    ql_nw_reg_state_e data_reg;
    int act;
    unsigned char csq;
    char operator_name[SYS_OPER_LEN];
    char mcc[SYS_MCC_LEN];
    char mnc[SYS_MNC_LEN];

    int profile_idx;
    int ip_version;
    int data_state;
    char ip[SYS_IP_LEN];
    char pri_dns[SYS_IP_LEN];
    char sec_dns[SYS_IP_LEN];
    int internet_ready;

    ql_rtc_time_t rtc;
    int64_t timestamp;
    uint32_t lifetime;

    mqtt_stat_t *mqtt;
} sys_info_t;

/** Global status snapshot. Read after calling sys_get(). */
extern sys_info_t sys_info;

/**
 * @brief Convert SIM status enum to a short readable string.
 * @param status SIM card status from Quectel API.
 * @return Static string such as "ready", "no sim", "sim pin".
 */
const char *sys_sim_status_str(ql_sim_status_e status);

/**
 * @brief Convert network registration state to a short readable string.
 * @param state Voice/data registration state.
 * @return Static string such as "home", "search", "roam", "denied".
 */
const char *sys_reg_state_str(ql_nw_reg_state_e state);

/**
 * @brief Convert PDP data-call state to a short readable string.
 * @param data_state QL_PDP_ACTIVED or QL_PDP_DEACTIVED.
 * @return "up" if PDP is active, otherwise "down".
 */
const char *sys_data_state_str(int data_state);

/**
 * @brief Convert Quectel RTC calendar time to Unix seconds (UTC, no timezone).
 * @param ts Output epoch seconds.
 * @param rtc Time from ql_rtc_get_time (tm_year full year, tm_mon 1–12).
 * @return 0 on success, -1 if ts or rtc is NULL.
 */
int rtc_to_unix(int64_t *ts, const ql_rtc_time_t *rtc);

/**
 * @brief Refresh global sys_info (SIM, NW, IP, clock, lifetime).
 *
 * Preserves sys_info.mqtt pointer. Does not start data call.
 *
 * @return Pointer to sys_info on success, NULL on failure.
 */
sys_info_t *sys_get(void);

/**
 * @brief Register live MQTT stats pointer into sys_info.mqtt.
 * @param stat Pointer to mqtt_stat_t owned by mqtt_proc (or NULL to clear).
 */
void sys_set_mqtt_stat(mqtt_stat_t *stat);

/**
 * @brief Wait for network registration, then sync RTC from NITZ.
 *
 * @return 0 if network registered (NITZ may still fail), -1 if registration failed.
 */
int sys_bootstrap(void);

/**
 * @brief Bring up PDP/data call (internet) on default profile.
 *
 * @return 0 on success (PDP up with IP), -1 on failure.
 */
int sys_network_up(void);

/**
 * @brief Check whether internet is ready from last sys_get() snapshot.
 *
 * @return 1 if internet is ready, 0 otherwise.
 */
int sys_internet_ready(void);

#endif /* SYS_H */
