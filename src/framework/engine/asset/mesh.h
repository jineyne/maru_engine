#ifndef ENGINE_ASSET_MESH_H
#define ENGINE_ASSET_MESH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rhi_vertex_attr;

typedef uint32_t mesh_handle_t;
#define MESH_HANDLE_INVALID ((mesh_handle_t)0)

typedef struct mesh_desc {
    const void *vertices;
    size_t vertex_size;
    uint32_t vertex_count;

    const void *indices;
    uint32_t index_count;

    /* Vertex layout */
    const struct rhi_vertex_attr *attrs;
    uint32_t attr_count;
} mesh_desc_t;

/* System management */
int mesh_system_init(size_t capacity);
void mesh_system_shutdown(void);

/* Mesh operations */
mesh_handle_t mesh_create(const mesh_desc_t *desc);
void mesh_destroy(mesh_handle_t h);

/* Query */
uint32_t mesh_get_vertex_count(mesh_handle_t h);
uint32_t mesh_get_index_count(mesh_handle_t h);

/* Rendering */
struct rhi_cmd;
void mesh_bind(struct rhi_cmd *cmd, mesh_handle_t h);
void mesh_draw(struct rhi_cmd *cmd, mesh_handle_t h);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_ASSET_MESH_H */
