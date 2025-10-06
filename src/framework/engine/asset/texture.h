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

typedef struct texture_opts_t {
    int gen_mips;
} texture_opts_t;

texture_t *texture_create_from_data(int width, int height, const unsigned char *rgba_pixels, const texture_opts_t *opts);
void texture_destroy(texture_t *tex);
void *texture_get_rhi_handle(texture_t *tex);

#ifdef __cplusplus
}
#endif

#endif /* MARU_TEXTURE_H */
