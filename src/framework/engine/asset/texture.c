#include "texture.h"

#include "engine_context.h"
#include "log.h"
#include "mem/mem_diag.h"
#include "rhi/rhi.h"

#include <string.h>

extern engine_context_t g_ctx;

texture_t *texture_create_from_data(int width, int height, const unsigned char *rgba_pixels, const texture_opts_t *opts_in) {
    if (!rgba_pixels || width <= 0 || height <= 0) {
        ERROR("texture_create_from_data: invalid args");
        return NULL;
    }

    texture_opts_t opts = {.gen_mips = 1};
    if (opts_in) opts = *opts_in;

    rhi_texture_desc_t td;
    memset(&td, 0, sizeof(td));
    td.width = width;
    td.height = height;
    td.mip_levels = opts.gen_mips ? 1 : 0;
    td.format = RHI_FMT_RGBA8;
    td.usage = RHI_TEX_USAGE_SAMPLED | (opts.gen_mips ? RHI_TEX_USAGE_GEN_MIPS : 0);

    rhi_texture_t *rhi_tex = g_ctx.active_rhi->create_texture(g_ctx.active_device, &td, rgba_pixels);
    if (!rhi_tex) {
        ERROR("texture_create_from_data: RHI create_texture failed");
        return NULL;
    }

    texture_t *tex = (texture_t*) MARU_MALLOC(sizeof(texture_t));
    if (!tex) {
        g_ctx.active_rhi->destroy_texture(g_ctx.active_device, rhi_tex);
        return NULL;
    }
    tex->width = width;
    tex->height = height;
    tex->channels = 4;
    tex->internal = (void*) rhi_tex;

    DEBUG_LOG("texture_create_from_data: %dx%d", width, height);
    return tex;
}

void texture_destroy(texture_t *tex) {
    if (!tex) return;

    if (tex->internal) {
        rhi_texture_t *rhi_tex = (rhi_texture_t*) tex->internal;
        if (g_ctx.active_device && g_ctx.active_rhi && g_ctx.active_rhi->destroy_texture) {
            g_ctx.active_rhi->destroy_texture(g_ctx.active_device, rhi_tex);
        } else {
            MARU_FREE(rhi_tex);
        }
        tex->internal = NULL;
    }
    MARU_FREE(tex);
}

void *texture_get_rhi_handle(texture_t *tex) {
    if (!tex) return NULL;
    return tex->internal;
}
