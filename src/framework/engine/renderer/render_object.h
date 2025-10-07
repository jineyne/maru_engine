#ifndef MARU_RENDER_OBJECT_H
#define MARU_RENDER_OBJECT_H

#include <stdint.h>
#include "math/transform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t render_object_handle_t;
typedef uint32_t material_handle_t;
typedef uint32_t mesh_handle_t;

#define RENDER_OBJECT_HANDLE_INVALID ((render_object_handle_t)0)

typedef struct render_object {
    mesh_handle_t mesh;
    material_handle_t material;
    transform_t *transform;

    /* Extensions */
    uint32_t layer;
    uint8_t visible : 1;
    uint8_t cast_shadow : 1;
} render_object_t;

/* Lifecycle */
int render_object_system_init(size_t capacity);
void render_object_system_shutdown(void);

/* Object management */
render_object_handle_t render_object_create(void);
void render_object_destroy(render_object_handle_t handle);

/* Setters */
void render_object_set_mesh(render_object_handle_t handle, mesh_handle_t mesh);
void render_object_set_material(render_object_handle_t handle, material_handle_t material);
void render_object_set_transform(render_object_handle_t handle, transform_t *transform);
void render_object_set_visible(render_object_handle_t handle, uint8_t visible);
void render_object_set_layer(render_object_handle_t handle, uint32_t layer);

/* Getters */
render_object_t *render_object_get(render_object_handle_t handle);
const render_object_t *render_object_get_const(render_object_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* MARU_RENDER_OBJECT_H */
