#ifndef MARU_ASSET_IMPORTER_H
#define MARU_ASSET_IMPORTER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Asset Importer System
 *
 * Generic asset import interface that allows registration of specific importers
 * for different file formats (textures, meshes, audio, etc.)
 */

typedef struct asset_importer_vtable_t {
    const char *name; /* Importer name (e.g., "texture", "mesh_obj") */

    /**
     * Check if this importer supports the given file extension
     * @param ext File extension (e.g., ".png", ".obj")
     * @return 1 if supported, 0 otherwise
     */
    int (*check)(const char *ext);

    /**
     * Import asset from file
     * @param path Absolute or relative path to asset file
     * @param opts Importer-specific options (can be NULL)
     * @return Pointer to loaded asset (type-specific), NULL on failure
     */
    void * (*import)(const char *path, const void *opts);

    /**
     * Free imported asset
     * @param asset Asset pointer returned by import()
     */
    void (*free_asset)(void *asset);

    /**
     * (Optional) Get metadata without full import
     * @param path File path
     * @param out_meta Output metadata structure (importer-specific)
     * @return 0 on success, error code otherwise
     */
    int (*get_metadata)(const char *path, void *out_meta);
} asset_importer_vtable_t;

/**
 * Initialize importer system
 */
void asset_importer_init(void);

/**
 * Shutdown importer system
 */
void asset_importer_shutdown(void);

/**
 * Register a new importer
 * @param importer Pointer to importer vtable (must be static/global)
 * @return 0 on success, negative on error
 */
int asset_importer_register(const asset_importer_vtable_t *importer);

/**
 * Find appropriate importer for given file path
 * @param path File path (extension will be extracted)
 * @return Importer vtable or NULL if not found
 */
const asset_importer_vtable_t *asset_importer_find(const char *path);

/**
 * Auto-import asset using appropriate importer
 * @param path File path
 * @param opts Importer-specific options (can be NULL)
 * @return Loaded asset or NULL on failure
 */
void *asset_import(const char *path, const void *opts);

#ifdef __cplusplus
}
#endif

#endif /* MARU_ASSET_IMPORTER_H */
