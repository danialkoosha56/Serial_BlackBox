#ifndef IO_PROC_H
#define IO_PROC_H

#include <stdbool.h>
#include <stddef.h>

#include "ql_gpio.h"
#include "ql_adc.h"

/** Board wiring — change here if your hardware uses other pins. */
#define IO_PROC_OUTPUT_GPIO   GPIO_10
#define IO_PROC_INPUT_GPIO    GPIO_11
#define IO_PROC_ADC_CHANNEL   QL_ADC0_CHANNEL

#define IO_PROC_LOG_SIZE  128

void io_proc_init(void);

int io_proc_set_output(bool high, char *log, size_t log_len);
int io_proc_read_input(int *level, char *log, size_t log_len);
int io_proc_read_adc(int *millivolt, char *log, size_t log_len);

#endif /* IO_PROC_H */
