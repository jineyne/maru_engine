#include "handle_pool.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "mem/mem_diag.h"

typedef struct {
    uint8_t gen;
    uint8_t used;
    uint32_t next;
} slot_hdr_t;

struct handle_pool {
    size_t cap;
    size_t obj_size;
    size_t obj_align;
    size_t stride;
    size_t alive;
    uint32_t free_head;
    uint8_t *blob;
};


#define IDX_MASK  ((uint32_t)0x00FFFFFFu)
#define GEN_SHIFT (24u)

static inline uint32_t get_handle_index(handle_t h) {
    return (h & IDX_MASK);
}

static inline uint32_t get_handle_gen(handle_t h) {
    return (h >> GEN_SHIFT) & 0xFFu;
}

static inline handle_t make_handle(uint32_t idx, uint8_t gen) {
    handle_t v = (((handle_t) gen) << GEN_SHIFT) | (idx & IDX_MASK);
    if (v == HANDLE_INVALID) {
        ++v;
    }

    return v;
}

static inline slot_hdr_t *slot_hdr_at(const handle_pool_t *hp, uint32_t idx) {
    return (slot_hdr_t*) (hp->blob + (hp->obj_size + sizeof(slot_hdr_t)) * (size_t) idx);
}

static inline void *slot_obj_at(handle_pool_t *hp, uint32_t idx) {
    return (void*) (hp->blob + (hp->obj_size + sizeof(slot_hdr_t)) * (size_t) idx + sizeof(slot_hdr_t));
}

static int safe_mul(size_t a, size_t b, size_t *out) {
#if defined(__has_builtin)
#   if __has_builtin(__builtin_umull_overflow) || __has_builtin(__builtin_mul_overflow)
    if (__builtin_mul_overflow(a, b, out)) {
        return 0;
    }

    return 1;
#   endif
#endif
    if (a == 0 || b == 0) {
        *out = 0;
        return 1;
    }

    if (a > SIZE_MAX / b) return 0;
    *out = a * b;
    return 1;
}


handle_pool_t *handle_pool_create(size_t capacity, size_t obj_size) {
    if (capacity == 0 || obj_size == 0 || capacity > IDX_MASK) return NULL;
    handle_pool_t *hp = (handle_pool_t*) MARU_MALLOC(sizeof(*hp));
    if (!hp) return NULL;

    hp->cap = capacity;
    hp->obj_size = obj_size;
    hp->alive = 0;
    hp->blob = (uint8_t*) MARU_MALLOC((obj_size + sizeof(slot_hdr_t)) * capacity);
    if (!hp->blob) {
        MARU_FREE(hp);
        return NULL;
    }

    for (uint32_t i = 0; i < (uint32_t) capacity; ++i) {
        slot_hdr_t *h = slot_hdr_at(hp, i);
        h->gen = 1;
        h->used = 0;
        h->next = i + 1;
    }
    ((slot_hdr_t*) slot_hdr_at(hp, (uint32_t) capacity - 1))->next = (uint32_t) capacity; /* end */
    hp->free_head = 0;
    return hp;
}

void handle_pool_destroy(handle_pool_t *hp) {
    if (!hp) return;
    MARU_FREE(hp->blob);
    MARU_FREE(hp);
}

void handle_pool_reset(handle_pool_t *hp) {
    if (!hp) return;
    for (uint32_t i = 0; i < (uint32_t) hp->cap; ++i) {
        slot_hdr_t *h = slot_hdr_at(hp, i);
        h->used = 0;
        h->next = i + 1;
        h->gen = (uint8_t) (h->gen + 1);
        if (h->gen == 0) {
            h->gen = 1;
        }

#if defined(MARU_DEBUG)
        memset(slot_obj_at(hp, i), 0xDD, hp->obj_size);
#endif
    }

    ((slot_hdr_t*) slot_hdr_at(hp, (uint32_t) hp->cap - 1))->next = (uint32_t) hp->cap;
    hp->free_head = 0;
    hp->alive = 0;
}

handle_t handle_pool_alloc(handle_pool_t *hp, const void *init_data) {
    if (!hp || hp->free_head >= hp->cap) return HANDLE_INVALID;
    uint32_t idx = hp->free_head;
    slot_hdr_t *h = slot_hdr_at(hp, idx);
    hp->free_head = h->next;

    h->used = 1;
    void *obj = slot_obj_at(hp, idx);
    if (init_data) {
        memcpy(obj, init_data, hp->obj_size);
    } else {
        memset(obj, 0, hp->obj_size);
    }

    ++hp->alive;
    handle_t ret = make_handle(idx, h->gen);
    if (ret == HANDLE_INVALID) {
        h->gen = (uint8_t) (h->gen + 1);
        if (h->gen == 0) {
            h->gen = 1;
        }

        ret = make_handle(idx, h->gen);
    }

    return make_handle(idx, h->gen);
}

static inline int validate(handle_pool_t *hp, handle_t h, uint32_t *out_idx, slot_hdr_t **out_hdr) {
    if (!hp || h == HANDLE_INVALID) return 0;
    uint32_t idx = get_handle_index(h);
    uint32_t gen = get_handle_gen(h);

    if (idx >= hp->cap) return 0;
    slot_hdr_t *hdr = slot_hdr_at(hp, idx);

    if (!hdr->used) return 0;
    if (hdr->gen != (uint8_t) gen) return 0;
    if (out_idx) *out_idx = idx;
    if (out_hdr) *out_hdr = hdr;
    return 1;
}

void *handle_pool_get(handle_pool_t *hp, handle_t h) {
    uint32_t idx;
    if (!validate(hp, h, &idx, NULL)) return NULL;
    return slot_obj_at(hp, idx);
}

const void *handle_pool_get_const(handle_pool_t *hp, handle_t h) {
    uint32_t idx;
    if (!validate(hp, h, &idx, NULL)) return NULL;
    return slot_obj_at(hp, idx);
}

void handle_pool_free(handle_pool_t *hp, handle_t h) {
    uint32_t idx;
    slot_hdr_t *hdr;
    if (!validate(hp, h, &idx, &hdr)) return;
    hdr->used = 0;
    hdr->gen = (uint8_t) (hdr->gen + 1);
    if (hdr->gen == 0) hdr->gen = 1;

    hdr->next = hp->free_head;
    hp->free_head = idx;
    if (hp->alive) --hp->alive;
}

size_t handle_pool_capacity(handle_pool_t *hp) {
    return hp ? hp->cap : 0;
}

size_t handle_pool_alive_count(handle_pool_t *hp) {
    return hp ? hp->alive : 0;
}
