#include "log.h"

#include <stdarg.h>
#ifdef MARU_PLATFORM_WINDOWS
#include <windows.h>
#endif


static void platform_output(const char *msg) {
#ifdef MARU_PLATFORM_WINDOWS
    OutputDebugStringA(msg);
    fprintf(stderr, "%s", msg);
#elif defined(MARU_PLATFORM_ANDROID)
    __android_log_print(ANDROID_LOG_INFO, "Maru", "%s", msg);
#elif defined(MARU_PLATFORM_IOS)
    fprintf(stderr, "%s", msg); /* NSLog bridge omitted for skeleton */
#else
    fprintf(stderr, "%s", msg);
#endif
}


void maru_log(maru_log_level level, const char *fmt, ...) {
    char prefix[16] = {0};
    switch (level) {
    case MARU_LOG_INFO:
        snprintf(prefix, sizeof(prefix), "[INFO] ");
        break;
    case MARU_LOG_ERROR:
        snprintf(prefix, sizeof(prefix), "[ERROR] ");
        break;
    case MARU_LOG_FATAL:
        snprintf(prefix, sizeof(prefix), "[FATAL] ");
        break;
    case MARU_LOG_DEBUG:
        snprintf(prefix, sizeof(prefix), "[DEBUG] ");
        break;
    default:
        snprintf(prefix, sizeof(prefix), "[LOG] ");
        break;
    }

    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    char out[1200];
    snprintf(out, sizeof(out), "%s%s\n", prefix, buffer);
    platform_output(out);
}
