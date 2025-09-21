#ifndef MARU_TEXTURE_H
#define MARU_TEXTURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct texture_t {
    int width;
    int height;
    int channels;
    void *internal;
} texture_t;

typedef struct asset_texture_opts_t {
    int gen_mips;
    int flip_y;
    int force_rgba;
} asset_texture_opts_t;

texture_t *asset_load_texture(const char *relpath, const asset_texture_opts_t *opts);
void asset_free_texture(texture_t *tex);

void *asset_texture_get_rhi_handle(texture_t *tex);

#ifdef __cplusplus
}
#endif

#endif /* MARU_TEXTURE_H */
