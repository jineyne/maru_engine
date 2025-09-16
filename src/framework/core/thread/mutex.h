#ifndef MARU_MUTEX_H
#define MARU_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mutex mutex_t;

mutex_t *maru_mutex_create(void);
void maru_mutex_destroy(mutex_t *m);

void maru_mutex_lock(mutex_t *m);
void maru_mutex_unlock(mutex_t *m);

int maru_mutex_trylock(mutex_t *m);

#ifdef __cplusplus
}
#endif

#endif /* MARU_MUTEX_H */
