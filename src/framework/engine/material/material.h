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

typedef enum {
    MATERIAL_PARAM_FLOAT,
    MATERIAL_PARAM_VEC2,
    MATERIAL_PARAM_VEC3,
    MATERIAL_PARAM_VEC4,
    MATERIAL_PARAM_MAT4,
    MATERIAL_PARAM_TEXTURE
} material_param_type_e;

material_param_id material_param(const char *name);

int material_system_init(size_t capacity);
void material_system_shutdown(void);

typedef struct material_desc {
    const char *shader_path;
    const char *vs_entry;
    const char *ps_entry;
} material_desc_t;

material_handle_t material_create(const material_desc_t *desc);
void material_destroy(material_handle_t h);

void material_set_float(material_handle_t h, const char *name, float value);
void material_set_vec2(material_handle_t h, const char *name, const float *v);
void material_set_vec3(material_handle_t h, const char *name, const float *v);
void material_set_vec4(material_handle_t h, const char *name, const float *v);
void material_set_mat4(material_handle_t h, const char *name, const float *m);
void material_set_texture(material_handle_t h, const char *name, texture_handle_t tex);

/* Internal - used by renderer */
struct rhi_cmd;
void material_bind(struct rhi_cmd *cmd, material_handle_t h);

#ifdef __cplusplus
}
#endif
#endif /* ENGINE_MATERIAL_H */
