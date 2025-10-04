#include "importer.h"
#include "log.h"

#include <string.h>
#include <ctype.h>

#define MAX_IMPORTERS 32

typedef struct asset_importer_manager_t {
    const asset_importer_vtable_t *importers[MAX_IMPORTERS];
    int count;
} asset_importer_manager_t;

static asset_importer_manager_t g_importer_mgr = {0};

/**
 * Extract file extension from path
 * @param path File path
 * @param out_ext Output buffer for extension (includes dot, e.g., ".png")
 * @param ext_size Size of output buffer
 * @return 0 on success, -1 if no extension found
 */
static int extract_extension(const char *path, char *out_ext, size_t ext_size) {
    if (!path || !out_ext || ext_size == 0) {
        return -1;
    }

    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');

    /* Check if dot comes after last path separator */
    const char *last_sep = slash > backslash ? slash : backslash;
    if (!dot || (last_sep && dot < last_sep)) {
        return -1;
    }

    /* Copy extension and convert to lowercase */
    size_t len = strlen(dot);
    if (len >= ext_size) {
        return -1;
    }

    for (size_t i = 0; i < len; i++) {
        out_ext[i] = (char)tolower((unsigned char)dot[i]);
    }
    out_ext[len] = '\0';

    return 0;
}

void asset_importer_init(void) {
    memset(&g_importer_mgr, 0, sizeof(g_importer_mgr));
    INFO("Asset importer system initialized");
}

void asset_importer_shutdown(void) {
    g_importer_mgr.count = 0;
    INFO("Asset importer system shutdown");
}

int asset_importer_register(const asset_importer_vtable_t *importer) {
    if (!importer) {
        ERROR("Cannot register NULL importer");
        return -1;
    }

    if (!importer->name || !importer->check || !importer->import) {
        ERROR("Importer missing required functions");
        return -1;
    }

    if (g_importer_mgr.count >= MAX_IMPORTERS) {
        ERROR("Importer registry full (max %d)", MAX_IMPORTERS);
        return -1;
    }

    g_importer_mgr.importers[g_importer_mgr.count++] = importer;
    INFO("Registered importer: %s", importer->name);

    return 0;
}

const asset_importer_vtable_t* asset_importer_find(const char *path) {
    if (!path) {
        return NULL;
    }

    char ext[32];
    if (extract_extension(path, ext, sizeof(ext)) != 0) {
        WARN("No extension found in path: %s", path);
        return NULL;
    }

    for (int i = 0; i < g_importer_mgr.count; i++) {
        const asset_importer_vtable_t *imp = g_importer_mgr.importers[i];
        if (imp->check(ext)) {
            return imp;
        }
    }

    WARN("No importer found for extension: %s", ext);
    return NULL;
}

void* asset_import(const char *path, const void *opts) {
    const asset_importer_vtable_t *importer = asset_importer_find(path);
    if (!importer) {
        ERROR("Failed to find importer for: %s", path);
        return NULL;
    }

    void *asset = importer->import(path, opts);
    if (!asset) {
        ERROR("Import failed for: %s (using %s)", path, importer->name);
        return NULL;
    }

    INFO("Successfully imported: %s (using %s)", path, importer->name);
    return asset;
}
