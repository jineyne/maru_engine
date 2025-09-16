#include "time.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

uint64_t time_now_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000ULL / freq.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
#endif
}
