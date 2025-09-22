#ifndef ENGINE_MATERIAL_H
#define ENGINE_MATERIAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef uint32_t texture_handle_t;
#define TEX_HANDLE_INVALID ((texture_handle_t)0)

    typedef uint32_t material_handle_t;
#define MAT_HANDLE_INVALID ((material_handle_t)0)

    typedef uint32_t material_param_id;

    material_param_id material_param(const char* name);

    int  material_system_init(size_t capacity);
    void material_system_shutdown(void);

    typedef struct material_desc {
        const char* shader_path;
        const char* vs_entry;
        const char* ps_entry;
    } material_desc_t;

    material_handle_t material_create(const material_desc_t* desc);
    void              material_destroy(material_handle_t h);

    int material_set_matrix4(material_handle_t h, material_param_id id, const float* m16);
    int material_set_texture(material_handle_t h, material_param_id id, texture_handle_t tex);

    struct rhi_cmd; /* fwd */
    void renderer_bind_material(struct rhi_cmd* cmd, material_handle_t h);

#define MATID(name) material_param(name)
/* example: MATID("uMVP"), MATID("gAlbedo") */

#ifdef __cplusplus
}
#endif
#endif /* ENGINE_MATERIAL_H */
