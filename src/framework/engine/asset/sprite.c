#include "sprite.h"

#include "mesh.h"
#include "texture_manager.h"
#include "engine_context.h"
#include "rhi/rhi.h"
#include "log.h"
#include "mem/mem_diag.h"

#include <string.h>

extern engine_context_t g_ctx;

typedef struct sprite_s {
    mesh_handle_t mesh;
    texture_handle_t texture;
    float width;
    float height;
} sprite_t;

static struct {
    sprite_t *sprites;
    uint32_t *free_list;
    uint32_t capacity;
    uint32_t count;
    uint32_t free_count;
} g_sprite_pool;

int sprite_system_init(size_t capacity) {
    if (g_sprite_pool.sprites) {
        ERROR("sprite system already initialized");
        return -1;
    }

    g_sprite_pool.capacity = (uint32_t)capacity;
    g_sprite_pool.sprites = (sprite_t *)MARU_CALLOC(capacity, sizeof(sprite_t));
    g_sprite_pool.free_list = (uint32_t *)MARU_MALLOC(capacity * sizeof(uint32_t));

    if (!g_sprite_pool.sprites || !g_sprite_pool.free_list) {
        ERROR("sprite system allocation failed");
        if (g_sprite_pool.sprites) MARU_FREE(g_sprite_pool.sprites);
        if (g_sprite_pool.free_list) MARU_FREE(g_sprite_pool.free_list);
        return -1;
    }

    for (uint32_t i = 0; i < capacity; ++i) {
        g_sprite_pool.free_list[i] = capacity - 1 - i;
    }
    g_sprite_pool.free_count = capacity;
    g_sprite_pool.count = 0;

    INFO("sprite system initialized (capacity=%u)", capacity);
    return 0;
}

void sprite_system_shutdown(void) {
    if (!g_sprite_pool.sprites) return;

    for (uint32_t i = 0; i < g_sprite_pool.capacity; ++i) {
        sprite_t *s = &g_sprite_pool.sprites[i];
        if (s->mesh != MESH_HANDLE_INVALID) {
            mesh_destroy(s->mesh);
        }
    }

    MARU_FREE(g_sprite_pool.sprites);
    MARU_FREE(g_sprite_pool.free_list);
    memset(&g_sprite_pool, 0, sizeof(g_sprite_pool));

    INFO("sprite system shutdown");
}

sprite_handle_t sprite_create(const sprite_desc_t *desc) {
    if (!desc || desc->texture == TEX_HANDLE_INVALID) {
        ERROR("sprite_create: invalid desc");
        return SPRITE_HANDLE_INVALID;
    }

    if (g_sprite_pool.free_count == 0) {
        ERROR("sprite_create: pool exhausted");
        return SPRITE_HANDLE_INVALID;
    }

    /* Create quad mesh: position(3) + texcoord(2) + normal(3) */
    float hw = desc->width * 0.5f;
    float hh = desc->height * 0.5f;
    float vertices[] = {
        /* pos */          /* uv */       /* normal */
        -hw, -hh, 0.0f,    0.0f, 1.0f,    0, 0, 1,  /* bottom-left */
         hw, -hh, 0.0f,    1.0f, 1.0f,    0, 0, 1,  /* bottom-right */
         hw,  hh, 0.0f,    1.0f, 0.0f,    0, 0, 1,  /* top-right */
        -hw,  hh, 0.0f,    0.0f, 0.0f,    0, 0, 1,  /* top-left */
    };

    uint32_t indices[] = {0, 2, 1, 2, 0, 3};  /* CW winding */

    static const rhi_vertex_attr_t attrs[] = {
        {"POSITION", 0, RHI_VTX_F32x3, 0, 0},
        {"TEXCOORD", 0, RHI_VTX_F32x2, 0, (uint32_t)(sizeof(float) * 3)},
        {"NORMAL",   0, RHI_VTX_F32x3, 0, (uint32_t)(sizeof(float) * 5)},
    };

    mesh_desc_t mesh_desc = {0};
    mesh_desc.vertices = vertices;
    mesh_desc.vertex_size = sizeof(float) * 8;
    mesh_desc.vertex_count = 4;
    mesh_desc.indices = indices;
    mesh_desc.index_count = 6;
    mesh_desc.attrs = attrs;
    mesh_desc.attr_count = 3;

    mesh_handle_t mesh = mesh_create(&mesh_desc);
    if (mesh == MESH_HANDLE_INVALID) {
        ERROR("sprite_create: failed to create mesh");
        return SPRITE_HANDLE_INVALID;
    }

    uint32_t idx = g_sprite_pool.free_list[--g_sprite_pool.free_count];
    sprite_t *s = &g_sprite_pool.sprites[idx];
    memset(s, 0, sizeof(*s));

    s->mesh = mesh;
    s->texture = desc->texture;
    s->width = desc->width;
    s->height = desc->height;
    g_sprite_pool.count++;

    return (sprite_handle_t)(idx + 1);
}

void sprite_destroy(sprite_handle_t h) {
    if (h == SPRITE_HANDLE_INVALID) return;

    uint32_t idx = h - 1;
    if (idx >= g_sprite_pool.capacity) {
        ERROR("sprite_destroy: invalid handle");
        return;
    }

    sprite_t *s = &g_sprite_pool.sprites[idx];
    if (s->mesh != MESH_HANDLE_INVALID) {
        mesh_destroy(s->mesh);
    }

    memset(s, 0, sizeof(*s));
    g_sprite_pool.free_list[g_sprite_pool.free_count++] = idx;
    g_sprite_pool.count--;
}

void sprite_draw(struct rhi_cmd *cmd, sprite_handle_t h, float x, float y) {
    if (h == SPRITE_HANDLE_INVALID) return;

    uint32_t idx = h - 1;
    if (idx >= g_sprite_pool.capacity) {
        ERROR("sprite_draw: invalid handle");
        return;
    }

    sprite_t *s = &g_sprite_pool.sprites[idx];

    /* TODO: Apply transform (x, y) via constant buffer or push constants */

    /* Bind texture */
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;
    if (rhi && s->texture != TEX_HANDLE_INVALID) {
        rhi_texture_t *tex = tex_acquire_rhi(s->texture);
        if (tex) {
            rhi->cmd_bind_texture(cmd, tex, 0, RHI_STAGE_PS);
        }
    }

    mesh_bind(cmd, s->mesh);
    mesh_draw(cmd, s->mesh);
}
