#ifndef SMS_PROC_H
#define SMS_PROC_H

void sms_proc_init(void);

/** Send @p text as one or more GSM SMS replies to the last accepted sender. */
int sms_send_text(const char *text);

#endif /* SMS_PROC_H */
