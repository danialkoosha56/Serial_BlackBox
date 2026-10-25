/*================================================================
  Copyright (c) 2020 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
  Quectel Wireless Solution Proprietary and Confidential.
=================================================================*/
/*=================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

WHEN              WHO         WHAT, WHERE, WHY
------------     -------     -------------------------------------------------------------------------------
10/10/2020  marvin          create

=================================================================*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_common.h"
#include "ql_api_osi.h"
#include "ql_api_sms.h"
#include "ql_log.h"

#define SMS_TEST_PHONE_NUM   "+989360145151"
#define SMS_TEST_TEXT        "SMS test"
#define SMS_RX_QUEUE_LEN     16

typedef struct
{
	uint8_t nSim;
	uint8_t index;
} sms_rx_item_t;

ql_task_t sms_task = NULL;
ql_sem_t  sms_init_sem = NULL;
ql_queue_t sms_rx_queue = NULL;

void user_sms_event_callback(uint8_t nSim, int event_id, void *ctx)
{
	switch(event_id)
	{
		case QL_SMS_INIT_OK_IND:
		{
			QL_SMS_LOG("QL_SMS_INIT_OK_IND");
			ql_rtos_semaphore_release(sms_init_sem);
			break;
		}
		case QL_SMS_NEW_MSG_IND:
		{
			ql_sms_new_s *msg = (ql_sms_new_s *)ctx;
			sms_rx_item_t item;

			if(msg == NULL || sms_rx_queue == NULL){
				break;
			}

			item.nSim = nSim;
			item.index = msg->index;
			QL_SMS_LOG("new sms sim=%d index=%d mem=%d", nSim, msg->index, msg->mem);
			if(ql_rtos_queue_release(sms_rx_queue, sizeof(item), (uint8 *)&item, QL_NO_WAIT) != QL_OSI_SUCCESS){
				QL_SMS_LOG("sms rx queue full, index=%d", msg->index);
			}
			break;
		}
		case QL_SMS_MEM_FULL_IND:
		{
			ql_sms_new_s *msg = (ql_sms_new_s *)ctx;
			QL_SMS_LOG("QL_SMS_MEM_FULL_IND sim=%d memory=%d", nSim, msg ? msg->mem : 0);
			break;
		}
		default:
			break;
	}
}

static int sms_hex_nibble(char c)
{
	if(c >= '0' && c <= '9'){
		return c - '0';
	}
	if(c >= 'A' && c <= 'F'){
		return c - 'A' + 10;
	}
	if(c >= 'a' && c <= 'f'){
		return c - 'a' + 10;
	}
	return -1;
}

/* CSCS=UCS2 returns each character as 4 hex digits, e.g. 00410061... == "Aa". */
static int sms_is_ucs2_hex(const char *s)
{
	size_t n;
	size_t i;

	if(s == NULL){
		return 0;
	}
	n = strlen(s);
	if(n == 0 || (n % 4) != 0){
		return 0;
	}
	for(i = 0; i < n; i++){
		if(sms_hex_nibble(s[i]) < 0){
			return 0;
		}
	}
	return 1;
}

static void sms_ucs2_hex_to_utf8(char *s)
{
	size_t n;
	size_t i;
	size_t o = 0;

	if(!sms_is_ucs2_hex(s)){
		return;
	}

	n = strlen(s);
	for(i = 0; i < n; i += 4){
		unsigned int code = ((unsigned int)sms_hex_nibble(s[i]) << 12)
			| ((unsigned int)sms_hex_nibble(s[i + 1]) << 8)
			| ((unsigned int)sms_hex_nibble(s[i + 2]) << 4)
			| (unsigned int)sms_hex_nibble(s[i + 3]);

		if(code < 0x80){
			s[o++] = (char)code;
		}else if(code < 0x800){
			s[o++] = (char)(0xC0 | (code >> 6));
			s[o++] = (char)(0x80 | (code & 0x3F));
		}else{
			s[o++] = (char)(0xE0 | (code >> 12));
			s[o++] = (char)(0x80 | ((code >> 6) & 0x3F));
			s[o++] = (char)(0x80 | (code & 0x3F));
		}
	}
	s[o] = '\0';
}

static void sms_print_incoming(uint8_t nSim, uint8_t index)
{
	ql_sms_recv_s *sms_recv = (ql_sms_recv_s *)calloc(1, sizeof(ql_sms_recv_s));

	if(sms_recv == NULL){
		QL_SMS_LOG("calloc incoming sms FAIL");
		return;
	}

	ql_sms_set_code_mode(QL_CS_GSM);
	if(QL_SMS_SUCCESS == ql_sms_read_msg_ex(nSim, index, TEXT, sms_recv)){
		sms_ucs2_hex_to_utf8(sms_recv->oa);
		sms_ucs2_hex_to_utf8((char *)sms_recv->data);
		QL_SMS_LOG("SMS index=%d from=%s time=%02d/%02d/%02d %02d:%02d:%02d text=%s",
			sms_recv->index, sms_recv->oa,
			sms_recv->scts.uYear, sms_recv->scts.uMonth, sms_recv->scts.uDay,
			sms_recv->scts.uHour, sms_recv->scts.uMinute, sms_recv->scts.uSecond,
			sms_recv->data);
	}else{
		QL_SMS_LOG("read incoming sms index=%d FAIL", index);
	}

	free(sms_recv);
}

void sms_demo_task(void * param)
{
	uint8_t nSim = 0;
	uint8_t sms_init_done = false;
	ql_sms_mem_info_t sms_mem = {0};

	QL_SMS_LOG("enter");

	if(ql_rtos_semaphore_create(&sms_init_sem, 0) != QL_OSI_SUCCESS)
	{
		QL_SMS_LOG("sms_init_sem created failed");
		goto exit;
	}

	if(ql_rtos_queue_create(&sms_rx_queue, sizeof(sms_rx_item_t), SMS_RX_QUEUE_LEN) != QL_OSI_SUCCESS)
	{
		QL_SMS_LOG("sms_rx_queue created failed");
		goto exit;
	}

	ql_sms_callback_register(user_sms_event_callback);

	ql_sms_get_init_status(nSim, &sms_init_done);
	if(!sms_init_done)
	{
		if(ql_rtos_semaphore_wait(sms_init_sem, QL_WAIT_FOREVER)){
			QL_SMS_LOG("Waiting for SMS init timeout");
		}
	}

	if(QL_SMS_SUCCESS == ql_sms_set_storage(nSim, SM, SM, SM)){
		ql_sms_get_storage(nSim, &sms_mem);
		QL_SMS_LOG("storage set to SIM, mem1=%d mem2=%d mem3=%d", sms_mem.mem1, sms_mem.mem2, sms_mem.mem3);
	}else{
		QL_SMS_LOG("ql_sms_set_storage FAIL");
	}

	if(QL_SMS_SUCCESS == ql_sms_delete_msg_ex(nSim, 0, QL_SMS_DEL_ALL)){
		QL_SMS_LOG("delete all sms OK");
	}else{
		QL_SMS_LOG("delete all sms FAIL");
	}

	ql_sms_set_code_mode(QL_CS_GSM);
	if(QL_SMS_SUCCESS == ql_sms_send_msg(nSim, SMS_TEST_PHONE_NUM, SMS_TEST_TEXT, GSM)){
		QL_SMS_LOG("test sms sent to %s", SMS_TEST_PHONE_NUM);
	}else{
		QL_SMS_LOG("test sms send FAIL");
	}

	while(1)
	{
		sms_rx_item_t item;

		if(ql_rtos_queue_wait(sms_rx_queue, (uint8 *)&item, sizeof(item), QL_WAIT_FOREVER) != QL_OSI_SUCCESS){
			continue;
		}
		sms_print_incoming(item.nSim, item.index);
	}

exit:
	if(sms_init_sem){
		ql_rtos_semaphore_delete(sms_init_sem);
		sms_init_sem = NULL;
	}
	if(sms_rx_queue){
		ql_rtos_queue_delete(sms_rx_queue);
		sms_rx_queue = NULL;
	}
	ql_rtos_task_delete(NULL);
}


QlOSStatus ql_sms_app_init(void)
{
	QlOSStatus err = QL_OSI_SUCCESS;

	err = ql_rtos_task_create(&sms_task, 4096, APP_PRIORITY_NORMAL, "QsmsApp", sms_demo_task, NULL, 2);
	if(err != QL_OSI_SUCCESS)
	{
		QL_SMS_LOG("sms_task created failed, ret = 0x%x", err);
	}

	return err;
}
