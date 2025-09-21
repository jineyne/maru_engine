#ifndef MARU_HANDLE_POOL_H
#define MARU_HANDLE_POOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t handle_t;
#define HANDLE_INVALID ((handle_t)0)

typedef struct handle_pool handle_pool_t;

handle_pool_t *handle_pool_create(size_t capacity, size_t obj_size, size_t obj_align);
void handle_pool_destroy(handle_pool_t *hp);

void handle_pool_reset(handle_pool_t *hp);

handle_t handle_pool_alloc(handle_pool_t *hp, const void *init_data);

void *handle_pool_get(handle_pool_t *hp, handle_t h);
const void *handle_pool_get_const(const handle_pool_t *hp, handle_t h);

void handle_pool_free(handle_pool_t *hp, handle_t h);

/* Ελ°θ */
size_t handle_pool_capacity(const handle_pool_t *hp);
size_t handle_pool_alive_count(const handle_pool_t *hp);


#ifdef __cplusplus
}
#endif

#endif /* MARU_HANDLE_POOL_H */
