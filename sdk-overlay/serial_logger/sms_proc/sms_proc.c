#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_osi.h"
#include "ql_api_sms.h"
#include "dani_log.h"
#include "setting.h"
#include "command.h"
#include "sms_proc.h"

#define DANI_LOG_FRAME_ID  Window21
#define LOG(msg, ...)      DANI_LOG(DANI_LOG_FRAME_ID, QL_LOG_LEVEL_INFO, "sms_proc", msg, ##__VA_ARGS__)

#define SMS_TEST_TEXT        "Data Logger is ON"
#define SMS_RX_QUEUE_LEN     16
#define SMS_GSM_CHUNK        140
#define SMS_REPLY_PHONE_LEN  32

typedef struct
{
    uint8_t nSim;
    uint8_t index;
} sms_rx_item_t;

static ql_task_t s_task = NULL;
static ql_sem_t s_init_sem = NULL;
static ql_queue_t s_rx_queue = NULL;
static uint8_t s_reply_sim = 0;
static char s_reply_phone[SMS_REPLY_PHONE_LEN];

static int sms_hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

/* CSCS=UCS2 returns each character as 4 hex digits, e.g. 00410061... == "Aa". */
static int sms_is_ucs2_hex(const char *s)
{
    size_t n;
    size_t i;

    if (s == NULL)
        return 0;

    n = strlen(s);
    if (n == 0 || (n % 4) != 0)
        return 0;

    for (i = 0; i < n; i++)
    {
        if (sms_hex_nibble(s[i]) < 0)
            return 0;
    }
    return 1;
}

static void sms_ucs2_hex_to_utf8(char *s)
{
    size_t n;
    size_t i;
    size_t o = 0;

    if (!sms_is_ucs2_hex(s))
        return;

    n = strlen(s);
    for (i = 0; i < n; i += 4)
    {
        unsigned int code = ((unsigned int)sms_hex_nibble(s[i]) << 12)
            | ((unsigned int)sms_hex_nibble(s[i + 1]) << 8)
            | ((unsigned int)sms_hex_nibble(s[i + 2]) << 4)
            | (unsigned int)sms_hex_nibble(s[i + 3]);

        if (code < 0x80)
        {
            s[o++] = (char)code;
        }
        else if (code < 0x800)
        {
            s[o++] = (char)(0xC0 | (code >> 6));
            s[o++] = (char)(0x80 | (code & 0x3F));
        }
        else
        {
            s[o++] = (char)(0xE0 | (code >> 12));
            s[o++] = (char)(0x80 | ((code >> 6) & 0x3F));
            s[o++] = (char)(0x80 | (code & 0x3F));
        }
    }
    s[o] = '\0';
}

static void sms_phone_key(const char *src, char *dst, size_t dst_size)
{
    char digits[SMS_REPLY_PHONE_LEN];
    size_t n = 0;
    const char *p;

    if (dst == NULL || dst_size == 0)
        return;

    dst[0] = '\0';
    if (src == NULL)
        return;

    for (; *src != '\0' && n + 1 < sizeof(digits); src++)
    {
        if (*src >= '0' && *src <= '9')
            digits[n++] = *src;
    }
    digits[n] = '\0';

    p = digits;
    if (n >= 4 && p[0] == '0' && p[1] == '0')
        p += 2;
    if (p[0] == '9' && p[1] == '8' && strlen(p) > 10)
        p += 2;
    if (p[0] == '0')
        p += 1;

    strncpy(dst, p, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static int sms_phone_match(const char *incoming, const char *allowed)
{
    char a[SMS_REPLY_PHONE_LEN];
    char b[SMS_REPLY_PHONE_LEN];

    sms_phone_key(incoming, a, sizeof(a));
    sms_phone_key(allowed, b, sizeof(b));
    if (a[0] == '\0' || b[0] == '\0')
        return 0;
    return strcmp(a, b) == 0;
}

int sms_send_text(const char *text)
{
    size_t len;
    size_t off = 0;

    if (text == NULL || s_reply_phone[0] == '\0')
        return -1;

    len = strlen(text);
    if (len == 0)
        return -1;

    ql_sms_set_code_mode(QL_CS_GSM);
    while (off < len)
    {
        char chunk[SMS_GSM_CHUNK + 1];
        size_t n = len - off;

        if (n > SMS_GSM_CHUNK)
            n = SMS_GSM_CHUNK;
        memcpy(chunk, text + off, n);
        chunk[n] = '\0';

        if (ql_sms_send_msg(s_reply_sim, s_reply_phone, chunk, GSM) != QL_SMS_SUCCESS)
        {
            LOG("reply sms FAIL to %s", s_reply_phone);
            return -1;
        }
        off += n;
    }

    LOG("reply sms sent to %s", s_reply_phone);
    return 0;
}

static void sms_incoming_processor(uint8_t nSim, uint8_t index)
{
    ql_sms_recv_s *sms_recv = (ql_sms_recv_s *)calloc(1, sizeof(ql_sms_recv_s));

    if (sms_recv == NULL)
    {
        LOG("calloc incoming sms FAIL");
        return;
    }

    ql_sms_set_code_mode(QL_CS_GSM);
    if (QL_SMS_SUCCESS == ql_sms_read_msg_ex(nSim, index, TEXT, sms_recv))
    {
        sms_ucs2_hex_to_utf8(sms_recv->oa);
        sms_ucs2_hex_to_utf8((char *)sms_recv->data);
        LOG("SMS index=%d from=%s time=%02d/%02d/%02d %02d:%02d:%02d text=%s",
            sms_recv->index, sms_recv->oa,
            sms_recv->scts.uYear, sms_recv->scts.uMonth, sms_recv->scts.uDay,
            sms_recv->scts.uHour, sms_recv->scts.uMinute, sms_recv->scts.uSecond,
            sms_recv->data);

        if (!sms_phone_match(sms_recv->oa, get_phone_number()))
        {
            LOG("sms ignored, from=%s allowed=%s", sms_recv->oa, get_phone_number());
        }
        else if (sms_recv->data[0] != '\0')
        {
            strncpy(s_reply_phone, sms_recv->oa, sizeof(s_reply_phone) - 1);
            s_reply_phone[sizeof(s_reply_phone) - 1] = '\0';
            s_reply_sim = nSim;
            cmd_procc(CMD_SRC_SMS, sms_recv->data, (unsigned int)strlen((char *)sms_recv->data));
        }

        if (ql_sms_delete_msg(nSim, index) != QL_SMS_SUCCESS)
            LOG("delete sms index=%d FAIL", index);
    }
    else
    {
        LOG("read incoming sms index=%d FAIL", index);
    }

    free(sms_recv);
}

static void sms_event_cb(uint8_t nSim, int event_id, void *ctx)
{
    switch (event_id)
    {
    case QL_SMS_INIT_OK_IND:
        LOG("QL_SMS_INIT_OK_IND");
        ql_rtos_semaphore_release(s_init_sem);
        break;

    case QL_SMS_NEW_MSG_IND:
    {
        ql_sms_new_s *msg = (ql_sms_new_s *)ctx;
        sms_rx_item_t item;

        if (msg == NULL || s_rx_queue == NULL)
            break;

        item.nSim = nSim;
        item.index = msg->index;
        LOG("new sms sim=%d index=%d mem=%d", nSim, msg->index, msg->mem);
        if (ql_rtos_queue_release(s_rx_queue, sizeof(item), (uint8 *)&item, QL_NO_WAIT) != QL_OSI_SUCCESS)
            LOG("sms rx queue full, index=%d", msg->index);
        break;
    }

    case QL_SMS_MEM_FULL_IND:
    {
        ql_sms_new_s *msg = (ql_sms_new_s *)ctx;
        LOG("QL_SMS_MEM_FULL_IND sim=%d memory=%d", nSim, msg ? msg->mem : 0);
        break;
    }

    default:
        break;
    }
}

static void sms_proc(void *param)
{
    uint8_t nSim = 0;
    uint8_t sms_init_done = false;
    ql_sms_mem_info_t sms_mem = {0};

    (void)param;
    LOG("%s", __func__);
    LOG("=========================================");
    LOG("this window porccess the incoming sms \r\n");

    if (ql_rtos_semaphore_create(&s_init_sem, 0) != QL_OSI_SUCCESS)
    {
        LOG("sms init sem created failed");
        goto exit;
    }

    if (ql_rtos_queue_create(&s_rx_queue, sizeof(sms_rx_item_t), SMS_RX_QUEUE_LEN) != QL_OSI_SUCCESS)
    {
        LOG("sms rx queue created failed");
        goto exit;
    }

    ql_sms_callback_register(sms_event_cb);

    ql_sms_get_init_status(nSim, &sms_init_done);
    if (!sms_init_done)
    {
        if (ql_rtos_semaphore_wait(s_init_sem, QL_WAIT_FOREVER))
            LOG("Waiting for SMS init timeout");
    }

    if (QL_SMS_SUCCESS == ql_sms_set_storage(nSim, SM, SM, SM))
    {
        ql_sms_get_storage(nSim, &sms_mem);
        LOG("storage set to SIM, mem1=%d mem2=%d mem3=%d", sms_mem.mem1, sms_mem.mem2, sms_mem.mem3);
    }
    else
    {
        LOG("ql_sms_set_storage FAIL");
    }

    if (QL_SMS_SUCCESS == ql_sms_delete_msg_ex(nSim, 0, QL_SMS_DEL_ALL))
        LOG("delete all sms OK");
    else
        LOG("delete all sms FAIL");

    ql_sms_set_code_mode(QL_CS_GSM);
    if (get_phone_number()[0] == '\0')
    {
        LOG("test sms skipped, phone_number empty");
    }
    else if (QL_SMS_SUCCESS == ql_sms_send_msg(nSim, get_phone_number(), SMS_TEST_TEXT, GSM))
    {
        LOG("test sms sent to %s", get_phone_number());
    }
    else
    {
        LOG("test sms send FAIL, phone=%s", get_phone_number());
    }

    while (1)
    {
        sms_rx_item_t item;

        if (ql_rtos_queue_wait(s_rx_queue, (uint8 *)&item, sizeof(item), QL_WAIT_FOREVER) != QL_OSI_SUCCESS)
            continue;

        sms_incoming_processor(item.nSim, item.index);
    }

exit:
    if (s_init_sem)
    {
        ql_rtos_semaphore_delete(s_init_sem);
        s_init_sem = NULL;
    }
    if (s_rx_queue)
    {
        ql_rtos_queue_delete(s_rx_queue);
        s_rx_queue = NULL;
    }
    ql_rtos_task_delete(NULL);
}

void sms_proc_init(void)
{
    if (ql_rtos_task_create(&s_task, 4096 * 2, APP_PRIORITY_NORMAL, "sms_proc",
                            sms_proc, NULL, 2) != QL_OSI_SUCCESS)
        LOG("init fail: task");
}
