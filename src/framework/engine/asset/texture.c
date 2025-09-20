#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "../core/mem/mem_diag.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "asset/texture.h"

#include "asset.h"
#include "engine_context.h"
#include "fs/fs.h"
#include "fs/path.h"
#include "log.h"
#include "rhi/rhi.h"

extern engine_context_t g_ctx;

texture_t *asset_load_texture(const char *relpath, const asset_texture_opts_t *opts_in) {
    if (!relpath) {
        MR_LOG(ERROR, "asset_load_texture: invalid args");
        return NULL;
    }

    asset_texture_opts_t opts = {.gen_mips = 1, .flip_y = 1, .force_rgba = 1};
    if (opts_in) opts = *opts_in;

    char *filebuf = NULL;
    size_t filesz = 0;
    if (asset_read_all(relpath, &filebuf, &filesz) != MARU_OK || !filebuf || filesz == 0) {
        MR_LOG(ERROR, "asset_load_texture: failed to read %s", relpath);
        if (filebuf) MARU_FREE(filebuf);
        return NULL;
    }

    stbi_set_flip_vertically_on_load(opts.flip_y ? 1 : 0);

    int w = 0, h = 0, ch = 0;
    const int req_comp = opts.force_rgba ? 4 : 0;
    unsigned char *pixels = stbi_load_from_memory((const unsigned char*) filebuf, (int) filesz, &w, &h, &ch, req_comp);
    MARU_FREE(filebuf);

    if (!pixels || w <= 0 || h <= 0) {
        MR_LOG(ERROR, "asset_load_texture: stbi decode failed for %s (%s)", relpath, stbi_failure_reason());
        if (pixels) stbi_image_free(pixels);
        return NULL;
    }

    const int comp = req_comp ? req_comp : ch;
    unsigned char *upload_pixels = pixels;
    if (comp != 4) {
        size_t total = (size_t) w * (size_t) h;
        unsigned char *conv = (unsigned char*) MARU_MALLOC(total * 4);
        if (!conv) {
            stbi_image_free(pixels);
            MR_LOG(ERROR, "asset_load_texture: OOM converting to RGBA");
            return NULL;
        }
        for (size_t i = 0; i < total; ++i) {
            int si = (int) (i * comp);
            int di = (int) (i * 4);
            conv[di + 0] = pixels[si + 0];
            conv[di + 1] = (comp > 1) ? pixels[si + 1] : 0;
            conv[di + 2] = (comp > 2) ? pixels[si + 2] : 0;
            conv[di + 3] = (comp > 3) ? pixels[si + 3] : 255;
        }
        stbi_image_free(pixels);
        upload_pixels = conv;
    }

    rhi_texture_desc_t td;
    memset(&td, 0, sizeof(td));
    td.width = w;
    td.height = h;
    td.mip_levels = opts.gen_mips ? 1 : 0;
    td.format = RHI_FMT_RGBA8;
    td.usage = RHI_TEX_USAGE_SAMPLED | (opts.gen_mips ? RHI_TEX_USAGE_GEN_MIPS : 0);

    rhi_texture_t *rhi_tex = g_ctx.active_rhi->create_texture(g_ctx.active_device, &td, upload_pixels);
    if (upload_pixels != pixels) {
        MARU_FREE(upload_pixels);
    } else {
        stbi_image_free(pixels);
    }

    if (!rhi_tex) {
        MR_LOG(ERROR, "asset_load_texture: RHI create_texture failed for %s", relpath);
        return NULL;
    }

    texture_t *tex = (texture_t*) MARU_MALLOC(sizeof(texture_t));
    if (!tex) {
        g_ctx.active_rhi->destroy_texture(g_ctx.active_device, rhi_tex);
        return NULL;
    }
    tex->width = w;
    tex->height = h;
    tex->channels = 4;
    tex->internal = (void*) rhi_tex;

    DEBUG_LOG("asset_load_texture: loaded %s (%dx%d)", relpath, w, h);
    return tex;
}

void asset_free_texture(texture_t *tex) {
    if (!tex) return;

    if (tex->internal) {
        rhi_texture_t *rhi_tex = (rhi_texture_t *)tex->internal;
        if (g_ctx.active_device && g_ctx.active_rhi && g_ctx.active_rhi->destroy_texture) {
            g_ctx.active_rhi->destroy_texture(g_ctx.active_device, rhi_tex);
        } else {
            MARU_FREE(rhi_tex);
        }
        tex->internal = NULL;
    }
    MARU_FREE(tex);
}

void *asset_texture_get_rhi_handle(texture_t *tex) {
    if (!tex) return NULL;
    return tex->internal;
}