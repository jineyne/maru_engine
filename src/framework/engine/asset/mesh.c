#include "mesh.h"

#include "engine_context.h"
#include "rhi/rhi.h"
#include "log.h"
#include "mem/mem_diag.h"

#include <string.h>

extern engine_context_t g_ctx;

typedef struct mesh_s {
    struct rhi_buffer *vb;
    struct rhi_buffer *ib;
    uint32_t vertex_count;
    uint32_t index_count;
} mesh_t;

static struct {
    mesh_t *meshes;
    uint32_t *free_list;
    uint32_t capacity;
    uint32_t count;
    uint32_t free_count;
} g_mesh_pool;

int mesh_system_init(size_t capacity) {
    if (g_mesh_pool.meshes) {
        ERROR("mesh system already initialized");
        return -1;
    }

    g_mesh_pool.capacity = (uint32_t)capacity;
    g_mesh_pool.meshes = (mesh_t *)MARU_CALLOC(capacity, sizeof(mesh_t));
    g_mesh_pool.free_list = (uint32_t *)MARU_MALLOC(capacity * sizeof(uint32_t));

    if (!g_mesh_pool.meshes || !g_mesh_pool.free_list) {
        ERROR("mesh system allocation failed");
        if (g_mesh_pool.meshes) MARU_FREE(g_mesh_pool.meshes);
        if (g_mesh_pool.free_list) MARU_FREE(g_mesh_pool.free_list);
        return -1;
    }

    for (uint32_t i = 0; i < capacity; ++i) {
        g_mesh_pool.free_list[i] = capacity - 1 - i;
    }
    g_mesh_pool.free_count = capacity;
    g_mesh_pool.count = 0;

    INFO("mesh system initialized (capacity=%u)", capacity);
    return 0;
}

void mesh_system_shutdown(void) {
    if (!g_mesh_pool.meshes) return;

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;
    if (rhi) {
        for (uint32_t i = 0; i < g_mesh_pool.capacity; ++i) {
            mesh_t *m = &g_mesh_pool.meshes[i];
            if (m->vb) {
                rhi->destroy_buffer(g_ctx.active_device, m->vb);
            }
            if (m->ib) {
                rhi->destroy_buffer(g_ctx.active_device, m->ib);
            }
        }
    }

    MARU_FREE(g_mesh_pool.meshes);
    MARU_FREE(g_mesh_pool.free_list);
    memset(&g_mesh_pool, 0, sizeof(g_mesh_pool));

    INFO("mesh system shutdown");
}

mesh_handle_t mesh_create(const mesh_desc_t *desc) {
    if (!desc || !desc->vertices || desc->vertex_count == 0) {
        ERROR("mesh_create: invalid desc");
        return MESH_HANDLE_INVALID;
    }

    if (g_mesh_pool.free_count == 0) {
        ERROR("mesh_create: pool exhausted");
        return MESH_HANDLE_INVALID;
    }

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;
    if (!rhi) {
        ERROR("mesh_create: no active RHI");
        return MESH_HANDLE_INVALID;
    }

    uint32_t idx = g_mesh_pool.free_list[--g_mesh_pool.free_count];
    mesh_t *m = &g_mesh_pool.meshes[idx];
    memset(m, 0, sizeof(*m));

    /* Create vertex buffer */
    rhi_buffer_desc_t vbd = {0};
    vbd.size = desc->vertex_size * desc->vertex_count;
    vbd.usage = RHI_BUF_VERTEX;
    vbd.stride = (uint32_t)desc->vertex_size;
    m->vb = rhi->create_buffer(g_ctx.active_device, &vbd, desc->vertices);
    if (!m->vb) {
        ERROR("mesh_create: failed to create vertex buffer");
        g_mesh_pool.free_list[g_mesh_pool.free_count++] = idx;
        return MESH_HANDLE_INVALID;
    }

    /* Create index buffer if provided */
    if (desc->indices && desc->index_count > 0) {
        rhi_buffer_desc_t ibd = {0};
        ibd.size = sizeof(uint32_t) * desc->index_count;
        ibd.usage = RHI_BUF_INDEX;
        m->ib = rhi->create_buffer(g_ctx.active_device, &ibd, desc->indices);
        if (!m->ib) {
            ERROR("mesh_create: failed to create index buffer");
            rhi->destroy_buffer(g_ctx.active_device, m->vb);
            g_mesh_pool.free_list[g_mesh_pool.free_count++] = idx;
            return MESH_HANDLE_INVALID;
        }
    }

    m->vertex_count = desc->vertex_count;
    m->index_count = desc->index_count;
    g_mesh_pool.count++;

    return (mesh_handle_t)(idx + 1);
}

void mesh_destroy(mesh_handle_t h) {
    if (h == MESH_HANDLE_INVALID) return;

    uint32_t idx = h - 1;
    if (idx >= g_mesh_pool.capacity) {
        ERROR("mesh_destroy: invalid handle");
        return;
    }

    mesh_t *m = &g_mesh_pool.meshes[idx];
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    if (rhi) {
        if (m->ib) rhi->destroy_buffer(g_ctx.active_device, m->ib);
        if (m->vb) rhi->destroy_buffer(g_ctx.active_device, m->vb);
    }

    memset(m, 0, sizeof(*m));
    g_mesh_pool.free_list[g_mesh_pool.free_count++] = idx;
    g_mesh_pool.count--;
}

uint32_t mesh_get_vertex_count(mesh_handle_t h) {
    if (h == MESH_HANDLE_INVALID) return 0;
    uint32_t idx = h - 1;
    if (idx >= g_mesh_pool.capacity) return 0;
    return g_mesh_pool.meshes[idx].vertex_count;
}

uint32_t mesh_get_index_count(mesh_handle_t h) {
    if (h == MESH_HANDLE_INVALID) return 0;
    uint32_t idx = h - 1;
    if (idx >= g_mesh_pool.capacity) return 0;
    return g_mesh_pool.meshes[idx].index_count;
}

void mesh_bind(struct rhi_cmd *cmd, mesh_handle_t h) {
    if (h == MESH_HANDLE_INVALID) return;

    uint32_t idx = h - 1;
    if (idx >= g_mesh_pool.capacity) {
        ERROR("mesh_bind: invalid handle");
        return;
    }

    mesh_t *m = &g_mesh_pool.meshes[idx];
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    if (rhi && m->vb) {
        rhi->cmd_set_vertex_buffer(cmd, 0, m->vb);
        if (m->ib) {
            rhi->cmd_set_index_buffer(cmd, m->ib);
        }
    }
}

void mesh_draw(struct rhi_cmd *cmd, mesh_handle_t h) {
    if (h == MESH_HANDLE_INVALID) return;

    uint32_t idx = h - 1;
    if (idx >= g_mesh_pool.capacity) {
        ERROR("mesh_draw: invalid handle");
        return;
    }

    mesh_t *m = &g_mesh_pool.meshes[idx];
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    if (!rhi) return;

    if (m->ib && m->index_count > 0) {
        rhi->cmd_draw_indexed(cmd, m->index_count, 0, 0, 1);
    } else if (m->vertex_count > 0) {
        rhi->cmd_draw(cmd, m->vertex_count, 0, 1);
    }
}
