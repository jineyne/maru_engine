#include "../importer.h"
#include "../texture.h"
#include "../asset.h"
#include "log.h"

#include <string.h>
#include <ctype.h>

/**
 * Texture Importer
 * Supports: PNG, JPG, JPEG, TGA, BMP
 */

static int texture_check(const char *ext) {
    if (!ext) return 0;

    /* Extension already lowercase from importer manager */
    return (strcmp(ext, ".png") == 0 ||
            strcmp(ext, ".jpg") == 0 ||
            strcmp(ext, ".jpeg") == 0 ||
            strcmp(ext, ".tga") == 0 ||
            strcmp(ext, ".bmp") == 0);
}

static void* texture_import(const char *path, const void *opts) {
    const asset_texture_opts_t *tex_opts = (const asset_texture_opts_t*)opts;

    /* Use existing asset_load_texture implementation */
    texture_t *tex = asset_load_texture(path, tex_opts);

    if (!tex) {
        ERROR("Texture import failed: %s", path);
        return NULL;
    }

    return (void*)tex;
}

static void texture_free(void *asset) {
    if (!asset) return;
    asset_free_texture((texture_t*)asset);
}

static int texture_get_metadata(const char *path, void *out_meta) {
    /* TODO: Implement metadata extraction without full load */
    (void)path;
    (void)out_meta;
    return -1; /* Not implemented */
}

/* Public importer vtable */
const asset_importer_vtable_t g_texture_importer = {
    .name = "texture",
    .check = texture_check,
    .import = texture_import,
    .free_asset = texture_free,
    .get_metadata = texture_get_metadata
};
