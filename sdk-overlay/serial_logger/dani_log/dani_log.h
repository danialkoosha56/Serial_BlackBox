#ifndef DANI_LOG_H
#define DANI_LOG_H

#include <stdarg.h>
#include "ql_log.h"

#define DANI_LOG_OUTPUT_LEVEL   QL_LOG_LEVEL_INFO
#define DANI_LOG_BUF_SIZE       1024

/* 0 = plain text  |  1 = framed packet (200, 200+ID, payload, 250) */
#define DANI_LOG_USE_FRAME      1

#define Window11		1
#define Window12		5
#define Window13		2
#define Window14		3


#define Window21		6
#define Window22		7
#define Window23		13
#define Window24		9

#define Window31		12
#define Window32		14
#define Window33		15



#define DANI_LOG_FRAME_START    200
#define DANI_LOG_FRAME_END      250

#if DANI_LOG_USE_FRAME

#define DANI_LOG(frame_id, level, module_str, msg, ...)                \
    do {                                                               \
        if (DANI_LOG_OUTPUT_LEVEL >= (level))                          \
            dani_log_impl_framed((unsigned char)(frame_id),            \
                                 (msg), ##__VA_ARGS__);               \
        (void)(module_str);                                            \
    } while (0)

int dani_log_impl_framed(unsigned char frame_id, const char *fmt, ...);

#else

#define DANI_LOG(frame_id, level, module_str, msg, ...)                \
    do {                                                               \
        if (DANI_LOG_OUTPUT_LEVEL >= (level))                          \
            dani_log_impl((level), (module_str), __FUNCTION__,         \
                          __LINE__, (msg), ##__VA_ARGS__);             \
        (void)(frame_id);                                              \
    } while (0)

int dani_log_impl(int level, const char *module_str, const char *func,
                  int line, const char *fmt, ...);

#endif

int dani_log_init(void);

#endif /* DANI_LOG_H */
