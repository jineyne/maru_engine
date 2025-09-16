#ifndef MARU_LOG_H
#define MARU_LOG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MARU_LOG_INFO,
    MARU_LOG_ERROR,
    MARU_LOG_FATAL,
    MARU_LOG_DEBUG
} maru_log_level;

void maru_log(maru_log_level level, const char *fmt, ...);

#ifndef NDEBUG
#define DEBUG_ENABLED 1
#else
#define DEBUG_ENABLED 0
#endif

#define INFO(...)  maru_log(MARU_LOG_INFO, __VA_ARGS__)
#define ERROR(...) maru_log(MARU_LOG_ERROR, __VA_ARGS__)
#define FATAL(...) maru_log(MARU_LOG_FATAL, __VA_ARGS__)
#if DEBUG_ENABLED
#define DEBUG_LOG(...) maru_log(MARU_LOG_DEBUG, __VA_ARGS__)
#else
#define DEBUG_LOG(...) do { } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* MARU_LOG_H */