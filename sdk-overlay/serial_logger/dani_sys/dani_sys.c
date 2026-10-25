#include <string.h>

#include "ql_api_osi.h"
#include "ql_api_nw.h"
#include "ql_api_sim.h"
#include "ql_api_datacall.h"
#include "ql_api_rtc.h"
#include "dani_sys.h"

sys_info_t sys_info = {0};
static int64_t s_boot_ts = 0;

const char *sys_sim_status_str(ql_sim_status_e status)
{
    switch (status)
    {
    case QL_SIM_STATUS_READY:           return "ready";
    case QL_SIM_STATUS_NOSIM:           return "no sim";
    case QL_SIM_STATUS_SIMPIN:          return "sim pin";
    case QL_SIM_STATUS_SIMPUK:          return "sim puk";
    case QL_SIM_STATUS_SIM_PRESENT:     return "present";
    default:                            return "other";
    }
}

const char *sys_reg_state_str(ql_nw_reg_state_e state)
{
    switch (state)
    {
    case QL_NW_REG_STATE_NOT_REGISTERED:             return "not reg";
    case QL_NW_REG_STATE_HOME_NETWORK:               return "home";
    case QL_NW_REG_STATE_TRYING_ATTACH_OR_SEARCHING: return "search";
    case QL_NW_REG_STATE_DENIED:                     return "denied";
    case QL_NW_REG_STATE_ROAMING:                    return "roam";
    default:                                         return "unk";
    }
}

const char *sys_data_state_str(int data_state)
{
    return data_state == QL_PDP_ACTIVED ? "up" : "down";
}

static int rtc_from_unix(int64_t ts, ql_rtc_time_t *rtc)
{
    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int64_t day;
    long hms;
    int year;
    int mon;

    if (rtc == NULL)
        return -1;

    day = ts / 86400;
    hms = ts % 86400;

    rtc->tm_hour = (int)(hms / 3600);
    rtc->tm_min = (int)((hms % 3600) / 60);
    rtc->tm_sec = (int)(hms % 60);

    for (year = 1970; ; year++)
    {
        int leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
        int ydays = leap ? 366 : 365;

        if (day < ydays)
            break;
        day -= ydays;
    }
    rtc->tm_year = year;

    for (mon = 1; mon <= 12; mon++)
    {
        int dim = mdays[mon - 1];

        if (mon == 2 && ((rtc->tm_year % 4 == 0) && (rtc->tm_year % 100 != 0)))
            dim = 29;
        if (day < dim)
            break;
        day -= dim;
    }
    rtc->tm_mon = mon;
    rtc->tm_mday = (int)day + 1;
    rtc->tm_wday = 0;
    return 0;
}

int rtc_to_unix(int64_t *ts, const ql_rtc_time_t *rtc)
{
    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int year;
    int mon;
    int64_t days;

    if (ts == NULL || rtc == NULL)
        return -1;

    days = 0;
    for (year = 1970; year < rtc->tm_year; year++)
    {
        int leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
        days += leap ? 366 : 365;
    }

    for (mon = 1; mon < rtc->tm_mon; mon++)
    {
        int dim = mdays[mon - 1];

        if (mon == 2 && ((rtc->tm_year % 4 == 0) && (rtc->tm_year % 100 != 0)))
            dim = 29;
        days += dim;
    }
    days += rtc->tm_mday - 1;

    *ts = days * 86400 + rtc->tm_hour * 3600 + rtc->tm_min * 60 + rtc->tm_sec;
    return 0;
}

static int fill_sim(sys_info_t *info)
{
    info->sim_status = QL_SIM_STATUS_UNKNOW;

    if (ql_sim_get_card_status(SYS_SIM_ID, &info->sim_status) != QL_SIM_SUCCESS)
        return -1;

    if (ql_sim_get_iccid(SYS_SIM_ID, info->iccid, sizeof(info->iccid)) != QL_SIM_SUCCESS)
        info->iccid[0] = '\0';

    if (ql_sim_get_imsi(SYS_SIM_ID, info->imsi, sizeof(info->imsi)) != QL_SIM_SUCCESS)
        info->imsi[0] = '\0';

    return 0;
}

static int fill_network(sys_info_t *info)
{
    ql_nw_reg_status_info_s reg = {0};
    ql_nw_operator_info_s oper = {0};

    info->csq = 99;

    if (ql_nw_get_reg_status(SYS_SIM_ID, &reg) != QL_NW_SUCCESS)
        return -1;

    info->voice_reg = reg.voice_reg.state;
    info->data_reg = reg.data_reg.state;
    info->act = reg.data_reg.act;

    ql_nw_get_csq(SYS_SIM_ID, &info->csq);

    if (ql_nw_get_operator_name(SYS_SIM_ID, &oper) == QL_NW_SUCCESS)
    {
        strncpy(info->operator_name, oper.long_oper_name, sizeof(info->operator_name) - 1);
        strncpy(info->mcc, oper.mcc, sizeof(info->mcc) - 1);
        strncpy(info->mnc, oper.mnc, sizeof(info->mnc) - 1);
    }

    return 0;
}

static void fill_data(sys_info_t *info)
{
    ql_data_call_info_s dc = {0};

    info->profile_idx = SYS_PROFILE_IDX_DEFAULT;
    info->ip_version = QL_PDP_TYPE_IP_NONE;
    info->data_state = QL_PDP_DEACTIVED;
    info->internet_ready = 0;
    info->ip[0] = '\0';
    info->pri_dns[0] = '\0';
    info->sec_dns[0] = '\0';

    if (ql_get_data_call_info(SYS_SIM_ID, SYS_PROFILE_IDX_DEFAULT, &dc) != QL_DATACALL_SUCCESS)
        return;

    info->profile_idx = dc.profile_idx;
    info->ip_version = dc.ip_version;
    info->data_state = dc.v4.state;

    if (dc.v4.state != QL_PDP_ACTIVED)
        return;

    inet_ntop(AF_INET, &dc.v4.addr.ip, info->ip, sizeof(info->ip));
    inet_ntop(AF_INET, &dc.v4.addr.pri_dns, info->pri_dns, sizeof(info->pri_dns));
    inet_ntop(AF_INET, &dc.v4.addr.sec_dns, info->sec_dns, sizeof(info->sec_dns));

    if (info->ip[0] != '\0' && strcmp(info->ip, "0.0.0.0") != 0)
        info->internet_ready = 1;
}

static int fill_time(sys_info_t *info)
{
    int64_t up;

    info->timestamp = 0;
    info->lifetime = 0;

    if (ql_rtc_get_time(&info->rtc) != QL_RTC_SUCCESS)
        return -1;

    rtc_to_unix(&info->timestamp, &info->rtc);

    if (s_boot_ts > 0)
    {
        up = info->timestamp - s_boot_ts;
        if (up > 0)
            info->lifetime = (uint32_t)up;
    }

    return 0;
}

static int wait_network(unsigned int timeout_sec, int retry_cnt)
{
    int ret = -1;
    int i;

    if (retry_cnt <= 0)
        retry_cnt = 1;

    for (i = 0; i < retry_cnt; i++)
    {
        ret = ql_network_register_wait(SYS_SIM_ID, timeout_sec);
        if (ret == 0)
            return 0;
        ql_rtos_task_sleep_s(1);
    }

    return ret;
}

static int sync_time_nitz(unsigned int wait_sec)
{
    ql_nw_nitz_time_info_s nitz = {0};
    ql_rtc_time_t rtc = {0};
    unsigned int i;

    if (wait_sec == 0)
        wait_sec = 1;

    for (i = 0; i < wait_sec; i++)
    {
        if (ql_nw_get_nitz_time_info(&nitz) == QL_NW_SUCCESS && nitz.abs_time > 0)
        {
            if (rtc_from_unix(nitz.abs_time, &rtc) != 0)
                return -1;

            if (ql_rtc_set_time(&rtc) != QL_RTC_SUCCESS)
                return -1;

            s_boot_ts = nitz.abs_time;
            return 0;
        }
        ql_rtos_task_sleep_s(1);
    }

    if (ql_rtc_get_time(&rtc) == QL_RTC_SUCCESS)
    {
        rtc_to_unix(&s_boot_ts, &rtc);
        return -1;
    }

    return -1;
}

sys_info_t *sys_get(void)
{
    mqtt_stat_t *mqtt = sys_info.mqtt;

    memset(&sys_info, 0, sizeof(sys_info));
    sys_info.mqtt = mqtt;

    if (fill_sim(&sys_info) != 0)
        return NULL;

    fill_network(&sys_info);
    fill_data(&sys_info);
    fill_time(&sys_info);
    return &sys_info;
}

void sys_set_mqtt_stat(mqtt_stat_t *stat)
{
    sys_info.mqtt = stat;
}

int sys_bootstrap(void)
{
    if (wait_network(120, 10) != 0)
        return -1;

    sync_time_nitz(60);
    return 0;
}

int sys_internet_ready(void)
{
    return sys_info.internet_ready;
}

int sys_network_up(void)
{
    ql_data_call_info_s dc;
    char ip[SYS_IP_LEN] = {0};
    int ret;

    if (sys_internet_ready())
        return 0;

    if (wait_network(120, 10) != 0)
        return -1;

    ql_set_data_call_asyn_mode(SYS_SIM_ID, SYS_PROFILE_IDX_DEFAULT, 0);
    ret = ql_start_data_call(SYS_SIM_ID, SYS_PROFILE_IDX_DEFAULT, QL_PDP_TYPE_IP,
                             SYS_APN_DEFAULT, NULL, NULL, 0);
    if (ret != 0)
        return -1;

    memset(&dc, 0, sizeof(dc));
    if (ql_get_data_call_info(SYS_SIM_ID, SYS_PROFILE_IDX_DEFAULT, &dc) != QL_DATACALL_SUCCESS)
        return -1;

    if (dc.v4.state != QL_PDP_ACTIVED)
        return -1;

    inet_ntop(AF_INET, &dc.v4.addr.ip, ip, sizeof(ip));
    if (ip[0] == '\0' || strcmp(ip, "0.0.0.0") == 0)
        return -1;

    sys_get();
    return 0;
}
