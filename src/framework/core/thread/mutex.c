#include "mutex.h"

#include "mem/mem_diag.h"

#if defined(_WIN32)
#include <windows.h>
struct mutex { CRITICAL_SECTION cs; };
#elif defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
struct mutex { pthread_mutex_t m; };
#else
#error "Unsupported platform for maru_mutex"
#endif


mutex_t *maru_mutex_create(void) {
    mutex_t *m = (mutex_t*)MARU_MALLOC(sizeof(mutex_t));
    if (!m) {
        return NULL;
    }

#if defined(_WIN32)
    InitializeCriticalSection(&m->cs);
#elif defined(__APPLE__) || defined(__linux__)
    /* ±âº»(ºñÀç±Í) ¹ÂÅØ½º */
    if (pthread_mutex_init(&m->m, NULL) != 0) { free(m); return NULL; }
#endif
    return m;
}

void maru_mutex_destroy(mutex_t *m) {
    if (!m) return;

#if defined(_WIN32)
    DeleteCriticalSection(&m->cs);
#elif defined(__APPLE__) || defined(__linux__)
    pthread_mutex_destroy(&m->m);
#endif
    MARU_FREE(m);
}

void maru_mutex_lock(mutex_t *m) {
    if (!m) return;

#if defined(_WIN32)
    EnterCriticalSection(&m->cs);
#elif defined(__APPLE__) || defined(__linux__)
    pthread_mutex_lock(&m->m);
#endif
}

void maru_mutex_unlock(mutex_t *m) {
    if (!m) return;

#if defined(_WIN32)
    LeaveCriticalSection(&m->cs);
#elif defined(__APPLE__) || defined(__linux__)
    pthread_mutex_unlock(&m->m);
#endif
}

int maru_mutex_trylock(mutex_t *m) {
    if (!m) return 1;

#if defined(_WIN32)
    return TryEnterCriticalSection(&m->cs) ? 0 : 1;
#elif defined(__APPLE__) || defined(__linux__)
    return pthread_mutex_trylock(&m->m) == 0 ? 0 : 1;
#endif
}
