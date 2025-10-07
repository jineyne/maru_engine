#include "render_object.h"
#include "handle/handle_pool.h"
#include "material/material.h"
#include "log.h"
#include <string.h>

static handle_pool_t *g_render_object_pool = NULL;

int render_object_system_init(size_t capacity) {
    if (g_render_object_pool) {
        WARN("render_object_system already initialized");
        return 0;
    }

    g_render_object_pool = handle_pool_create(capacity, sizeof(render_object_t), (size_t) _Alignof(render_object_t));
    if (!g_render_object_pool) {
        ERROR("failed to create render_object handle pool");
        return -1;
    }

    INFO("render_object_system initialized (capacity: %zu)", capacity);
    return 0;
}

void render_object_system_shutdown(void) {
    if (g_render_object_pool) {
        handle_pool_destroy(g_render_object_pool);
        g_render_object_pool = NULL;
        INFO("render_object_system shutdown");
    }
}

render_object_handle_t render_object_create(void) {
    if (!g_render_object_pool) {
        ERROR("render_object_system not initialized");
        return RENDER_OBJECT_HANDLE_INVALID;
    }

    render_object_t init = {0};
    init.visible = 1;
    init.layer = 0;
    init.cast_shadow = 0;

    render_object_handle_t handle = handle_pool_alloc(g_render_object_pool, &init);
    if (handle == HANDLE_INVALID) {
        ERROR("failed to allocate render_object");
        return RENDER_OBJECT_HANDLE_INVALID;
    }

    return handle;
}

void render_object_destroy(render_object_handle_t handle) {
    if (!g_render_object_pool) return;
    if (handle == RENDER_OBJECT_HANDLE_INVALID) return;

    /* Cleanup material instance */
    render_object_t *obj = handle_pool_get(g_render_object_pool, handle);
    if (obj && obj->material != MAT_HANDLE_INVALID) {
        material_destroy(obj->material);
        obj->material = MAT_HANDLE_INVALID;
    }

    handle_pool_free(g_render_object_pool, handle);
}

void render_object_set_mesh(render_object_handle_t handle, mesh_handle_t mesh) {
    render_object_t *obj = handle_pool_get(g_render_object_pool, handle);
    if (obj) {
        obj->mesh = mesh;
    }
}

void render_object_set_material(render_object_handle_t handle, material_handle_t base_material) {
    render_object_t *obj = handle_pool_get(g_render_object_pool, handle);
    if (!obj) return;

    /* Destroy existing instance if any */
    if (obj->material != MAT_HANDLE_INVALID) {
        material_destroy(obj->material);
    }

    /* Create new instance from base material */
    obj->material = material_create_instance(base_material);
    if (obj->material == MAT_HANDLE_INVALID) {
        ERROR("render_object_set_material: failed to create material instance");
    }
}

void render_object_set_transform(render_object_handle_t handle, transform_t *transform) {
    render_object_t *obj = handle_pool_get(g_render_object_pool, handle);
    if (obj) {
        obj->transform = transform;
    }
}

void render_object_set_visible(render_object_handle_t handle, uint8_t visible) {
    render_object_t *obj = handle_pool_get(g_render_object_pool, handle);
    if (obj) {
        obj->visible = visible ? 1 : 0;
    }
}

void render_object_set_layer(render_object_handle_t handle, uint32_t layer) {
    render_object_t *obj = handle_pool_get(g_render_object_pool, handle);
    if (obj) {
        obj->layer = layer;
    }
}

render_object_t *render_object_get(render_object_handle_t handle) {
    if (!g_render_object_pool) return NULL;
    return (render_object_t *)handle_pool_get(g_render_object_pool, handle);
}

const render_object_t *render_object_get_const(render_object_handle_t handle) {
    if (!g_render_object_pool) return NULL;
    return (const render_object_t *)handle_pool_get_const(g_render_object_pool, handle);
}
