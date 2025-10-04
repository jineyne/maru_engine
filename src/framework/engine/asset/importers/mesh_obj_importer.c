#include "../importer.h"
#include "../mesh.h"
#include "../asset.h"
#include "log.h"
#include "mem/mem_diag.h"
#include "rhi/rhi.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * Simple OBJ Mesh Importer
 * Supports: position, texcoord, normal (triangulated faces only)
 */

typedef struct vec3_t {
    float x, y, z;
} vec3_t;

typedef struct vec2_t {
    float x, y;
} vec2_t;

typedef struct vertex_t {
    vec3_t position;
    vec2_t texcoord;
    vec3_t normal;
} vertex_t;

typedef struct obj_data_t {
    vec3_t *positions;
    vec2_t *texcoords;
    vec3_t *normals;
    vertex_t *vertices;
    uint32_t *indices;

    uint32_t pos_count;
    uint32_t tex_count;
    uint32_t norm_count;
    uint32_t vert_count;
    uint32_t idx_count;

    uint32_t pos_cap;
    uint32_t tex_cap;
    uint32_t norm_cap;
    uint32_t vert_cap;
    uint32_t idx_cap;
} obj_data_t;

static void obj_data_init(obj_data_t *obj) {
    memset(obj, 0, sizeof(*obj));
    obj->pos_cap = 1024;
    obj->tex_cap = 1024;
    obj->norm_cap = 1024;
    obj->vert_cap = 2048;
    obj->idx_cap = 4096;

    obj->positions = MARU_MALLOC(obj->pos_cap * sizeof(vec3_t));
    obj->texcoords = MARU_MALLOC(obj->tex_cap * sizeof(vec2_t));
    obj->normals = MARU_MALLOC(obj->norm_cap * sizeof(vec3_t));
    obj->vertices = MARU_MALLOC(obj->vert_cap * sizeof(vertex_t));
    obj->indices = MARU_MALLOC(obj->idx_cap * sizeof(uint32_t));
}

static void obj_data_free(obj_data_t *obj) {
    if (obj->positions)
        MARU_FREE(obj->positions);
    if (obj->texcoords)
        MARU_FREE(obj->texcoords);
    if (obj->normals)
        MARU_FREE(obj->normals);
    if (obj->vertices)
        MARU_FREE(obj->vertices);
    if (obj->indices)
        MARU_FREE(obj->indices);
    memset(obj, 0, sizeof(*obj));
}

static int parse_obj(const char *data, obj_data_t *obj) {
    if (!data || !obj) return -1;

    obj_data_init(obj);

    char line[512];
    const char *ptr = data;

    while (*ptr) {
        /* Read line */
        int i = 0;
        while (*ptr && *ptr != '\n' && i < (int) sizeof(line) - 1) {
            line[i++] = *ptr++;
        }

        line[i] = '\0';
        if (*ptr == '\n') ptr++;

        /* Skip empty or comment */
        if (line[0] == '\0' || line[0] == '#') continue;

        /* Parse vertex position: v x y z */
        if (line[0] == 'v' && line[1] == ' ') {
            vec3_t v;
            if (sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z) == 3) {
                if (obj->pos_count >= obj->pos_cap) {
                    ERROR("OBJ: too many positions");
                    return -1;
                }
                obj->positions[obj->pos_count++] = v;
            }
        }
        /* Parse texture coord: vt u v */
        else if (line[0] == 'v' && line[1] == 't' && line[2] == ' ') {
            vec2_t vt;
            if (sscanf(line + 3, "%f %f", &vt.x, &vt.y) == 2) {
                if (obj->tex_count >= obj->tex_cap) {
                    ERROR("OBJ: too many texcoords");
                    return -1;
                }
                obj->texcoords[obj->tex_count++] = vt;
            }
        }
        /* Parse normal: vn x y z */
        else if (line[0] == 'v' && line[1] == 'n' && line[2] == ' ') {
            vec3_t vn;
            if (sscanf(line + 3, "%f %f %f", &vn.x, &vn.y, &vn.z) == 3) {
                if (obj->norm_count >= obj->norm_cap) {
                    ERROR("OBJ: too many normals");
                    return -1;
                }
                obj->normals[obj->norm_count++] = vn;
            }
        }
        /* Parse face: f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3 */
        else if (line[0] == 'f' && line[1] == ' ') {
            int v[3], vt[3], vn[3];
            int matched = sscanf(line + 2, "%d/%d/%d %d/%d/%d %d/%d/%d",
                                 &v[0], &vt[0], &vn[0],
                                 &v[1], &vt[1], &vn[1],
                                 &v[2], &vt[2], &vn[2]);

            if (matched == 9) {
                /* Create vertices for triangle */
                for (int j = 0; j < 3; j++) {
                    if (obj->vert_count >= obj->vert_cap || obj->idx_count >= obj->idx_cap) {
                        ERROR("OBJ: too many vertices/indices");
                        return -1;
                    }

                    vertex_t vert = {0};

                    /* OBJ indices are 1-based */
                    int pidx = v[j] - 1;
                    int tidx = vt[j] - 1;
                    int nidx = vn[j] - 1;

                    if (pidx >= 0 && pidx < (int) obj->pos_count) {
                        vert.position = obj->positions[pidx];
                    }
                    if (tidx >= 0 && tidx < (int) obj->tex_count) {
                        vert.texcoord = obj->texcoords[tidx];
                    }
                    if (nidx >= 0 && nidx < (int) obj->norm_count) {
                        vert.normal = obj->normals[nidx];
                    }

                    obj->vertices[obj->vert_count] = vert;
                    obj->indices[obj->idx_count++] = obj->vert_count;
                    obj->vert_count++;
                }
            }
        }
    }

    INFO("OBJ parsed: %u vertices, %u indices", obj->vert_count, obj->idx_count);
    return 0;
}

static int mesh_obj_check(const char *ext) {
    if (!ext) return 0;
    return strcmp(ext, ".obj") == 0;
}

static void *mesh_obj_import(const char *path, const void *opts) {
    UNUSED(opts);

    /* Read OBJ file */
    size_t data_size;
    char *data = asset_read_all(path, &data_size, 1);
    if (!data) {
        ERROR("Failed to read OBJ file: %s", path);
        return NULL;
    }

    /* Parse OBJ */
    obj_data_t obj;
    if (parse_obj(data, &obj) != 0) {
        ERROR("Failed to parse OBJ: %s", path);
        MARU_FREE(data);
        return NULL;
    }
    MARU_FREE(data);

    /* Create mesh descriptor */
    static const rhi_vertex_attr_t attrs[] = {
        {"POSITION", 0, RHI_VTX_F32x3, 0, offsetof(vertex_t, position)},
        {"TEXCOORD", 1, RHI_VTX_F32x2, 0, offsetof(vertex_t, texcoord)},
        {"NORMAL", 2, RHI_VTX_F32x3, 0, offsetof(vertex_t, normal)}
    };

    mesh_desc_t desc = {0};
    desc.vertices = obj.vertices;
    desc.vertex_size = sizeof(vertex_t);
    desc.vertex_count = obj.vert_count;
    desc.indices = obj.indices;
    desc.index_count = obj.idx_count;
    desc.attrs = attrs;
    desc.attr_count = 3;

    /* Create mesh */
    mesh_handle_t handle = mesh_create(&desc);

    /* Free temporary data */
    obj_data_free(&obj);

    if (handle == MESH_HANDLE_INVALID) {
        ERROR("Failed to create mesh from OBJ: %s", path);
        return NULL;
    }

    /* Return handle as pointer (need wrapper struct for proper handling) */
    mesh_handle_t *h = MARU_MALLOC(sizeof(mesh_handle_t));
    *h = handle;

    return (void*) h;
}

static void mesh_obj_free(void *asset) {
    if (!asset) return;

    mesh_handle_t *h = (mesh_handle_t*) asset;
    mesh_destroy(*h);
    MARU_FREE(h);
}

/* Public importer vtable */
const asset_importer_vtable_t g_mesh_obj_importer = {
    .name = "mesh_obj",
    .check = mesh_obj_check,
    .import = mesh_obj_import,
    .free_asset = mesh_obj_free,
    .get_metadata = NULL
};
