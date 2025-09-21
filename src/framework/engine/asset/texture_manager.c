#include "texture_manager.h"

#include "handle/handle_pool.h"
#include "mem/mem_diag.h"
#include "log.h"

#include <string.h>

typedef struct tex_rec_s {
    texture_t *tex;
    char *src_rel;
    uint32_t flags;
} tex_rec_t;

static handle_pool_t *s_pool = NULL;

static char *dup_cstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char*) MARU_MALLOC(n);
    if (p) memcpy(p, s, n);
    return p;
}


int texture_manager_init(size_t capacity) {
    if (s_pool) return 0;
    if (capacity == 0) capacity = 256;

    s_pool = handle_pool_create(capacity, sizeof(tex_rec_t), (size_t) _Alignof(tex_rec_t));
    if (!s_pool) {
        MR_LOG(FATAL, "texture_manager: handle pool create failed (cap=%zu)", capacity);
        return -1;
    }

    return 0;
}

void texture_manager_shutdown(void) {
    if (!s_pool) return;

    size_t cap = handle_pool_capacity(s_pool);
    for (uint32_t idx = 0; idx < (uint32_t) cap; ++idx) {
        tex_rec_t *rec = (tex_rec_t*) handle_pool_get(s_pool, make_handle(idx, ((uint8_t) 0xFF)));
        if (!rec) continue;
    }

    handle_pool_destroy(s_pool);
    s_pool = NULL;
}


texture_handle_t tex_create_from_file(const char *relpath, const asset_texture_opts_t *opts) {
    if (!s_pool || !relpath) return TEX_HANDLE_INVALID;

    asset_texture_opts_t o = {.gen_mips = 1, .flip_y = 1, .force_rgba = 1};
    if (opts) o = *opts;

    texture_t *t = asset_load_texture(relpath, &o);
    if (!t) {
        MR_LOG(ERROR, "texture_manager: load failed: %s", relpath);
        return TEX_HANDLE_INVALID;
    }

    tex_rec_t init = {0};
    init.tex = t;
    init.src_rel = dup_cstr(relpath);
    init.flags = TEX_STATE_READY;
    handle_t h = handle_pool_alloc(s_pool, &init);
    if (h == HANDLE_INVALID) {
        MR_LOG(ERROR, "texture_manager: pool full; cannot allocate handle for %s", relpath);
        asset_free_texture(t);
        if (init.src_rel) {
            MARU_FREE(init.src_rel);
        }

        return TEX_HANDLE_INVALID;
    }

    return (texture_handle_t) h;
}

void tex_destroy(texture_handle_t th) {
    if (!s_pool || th == TEX_HANDLE_INVALID) return;
    tex_rec_t *rec = (tex_rec_t*) handle_pool_get(s_pool, (handle_t) th);
    if (!rec) return;


    if (rec->tex) {
        asset_free_texture(rec->tex);
        rec->tex = NULL;
    }
    if (rec->src_rel) {
        MARU_FREE(rec->src_rel);
        rec->src_rel = NULL;
    }
    rec->flags = TEX_STATE_EMPTY;

    handle_pool_free(s_pool, (handle_t) th);
}


rhi_texture_t *tex_acquire_rhi(texture_handle_t th) {
    if (!s_pool || th == TEX_HANDLE_INVALID) return NULL;

    tex_rec_t *rec = (tex_rec_t*) handle_pool_get(s_pool, (handle_t) th);
    if (!rec || !(rec->flags & TEX_STATE_READY) || !rec->tex) return NULL;

    return (rhi_texture_t*) rec->tex->internal;
}


int tex_is_ready(texture_handle_t th) {
    if (!s_pool || th == TEX_HANDLE_INVALID) return 0;

    tex_rec_t *rec = (tex_rec_t*) handle_pool_get(s_pool, (handle_t) th);
    return (rec && (rec->flags & TEX_STATE_READY) && rec->tex) ? 1 : 0;
}


const char *tex_get_source_path(texture_handle_t th) {
    if (!s_pool || th == TEX_HANDLE_INVALID) return NULL;

    tex_rec_t *rec = (tex_rec_t*) handle_pool_get(s_pool, (handle_t) th);
    return rec ? rec->src_rel : NULL;
}
