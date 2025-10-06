#include "asset/importer.h"
#include "asset/texture.h"
#include "asset/asset.h"
#include "engine_context.h"
#include "log.h"
#include "mem/mem_diag.h"
#include "rhi/rhi.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <string.h>

extern engine_context_t g_ctx;

typedef struct texture_import_opts_t {
    int gen_mips;
    int flip_y_override;  /* -1: auto, 0: no flip, 1: flip */
} texture_import_opts_t;

static int texture_check(const char *ext) {
    if (!ext) return 0;
    return (strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0 ||
            strcmp(ext, ".jpeg") == 0 || strcmp(ext, ".tga") == 0 ||
            strcmp(ext, ".bmp") == 0);
}

static void *texture_import(const char *path, const void *opts_in) {
    texture_import_opts_t opts = {.gen_mips = 1, .flip_y_override = -1};
    if (opts_in) opts = *(const texture_import_opts_t*) opts_in;

    /* Determine flip_y based on RHI conventions */
    int flip_y = 0;
    if (opts.flip_y_override >= 0) {
        flip_y = opts.flip_y_override;
    } else if (g_ctx.active_rhi && g_ctx.active_device) {
        rhi_capabilities_t caps;
        g_ctx.active_rhi->get_capabilities(g_ctx.active_device, &caps);
        /* OpenGL (UV origin bottom-left) needs flip, DirectX (UV origin top-left) doesn't */
        flip_y = (caps.conventions.uv_yaxis == RHI_AXIS_UP) ? 1 : 0;
    }

    /* Read file */
    size_t buf_len;
    char *buf = asset_read_all(path, &buf_len, 1);
    if (!buf) {
        ERROR("Failed to read texture file: %s", path);
        return NULL;
    }

    /* Decode image */
    stbi_set_flip_vertically_on_load(flip_y ? 1 : 0);
    int w = 0, h = 0, ch = 0;
    unsigned char *pixels = stbi_load_from_memory((const unsigned char*) buf, (int) buf_len, &w, &h, &ch, 4);
    MARU_FREE(buf);

    if (!pixels || w <= 0 || h <= 0) {
        ERROR("stbi decode failed for %s: %s", path, stbi_failure_reason());
        if (pixels) stbi_image_free(pixels);
        return NULL;
    }

    /* Create texture from RGBA pixels */
    texture_opts_t tex_opts = {.gen_mips = opts.gen_mips};
    texture_t *tex = texture_create_from_data(w, h, pixels, &tex_opts);
    stbi_image_free(pixels);

    if (!tex) {
        ERROR("Texture import failed: %s", path);
        return NULL;
    }

    INFO("Texture imported: %s (%dx%d)", path, w, h);
    return (void*) tex;
}

static void texture_free(void *asset) {
    if (!asset) return;
    texture_destroy((texture_t*) asset);
}

static int texture_get_metadata(const char *path, void *out_meta) {
    UNUSED(path);
    UNUSED(out_meta);
    return -1;
}

const asset_importer_vtable_t g_texture_importer = {
    .name = "texture",
    .check = texture_check,
    .import = texture_import,
    .free_asset = texture_free,
    .get_metadata = texture_get_metadata
};
