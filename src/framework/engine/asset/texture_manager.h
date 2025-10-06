#ifndef MARU_TEXTURE_MANAGER_H
#define MARU_TEXTURE_MANAGER_H


#include "core.h"
#include "rhi/rhi.h"
#include "asset/texture.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t texture_handle_t;
#define TEX_HANDLE_INVALID ((texture_handle_t)0)

enum {
    TEX_STATE_EMPTY = 0,
    TEX_STATE_READY = 1 << 0,
    TEX_STATE_LOADING = 1 << 1,
    TEX_STATE_FAILED = 1 << 2,
};

int texture_manager_init(size_t capacity);
void texture_manager_shutdown(void);

texture_handle_t tex_create_from_file(const char *relpath);

void tex_destroy(texture_handle_t h);

rhi_texture_t *tex_acquire_rhi(texture_handle_t h);

int tex_is_ready(texture_handle_t h);
const char *tex_get_source_path(texture_handle_t h);


#ifdef __cplusplus
}
#endif


#endif /* MARU_TEXTURE_MANAGER_H */
