#include "material.h"

#include "engine_context.h"
#include "rhi/rhi.h"
#include "asset/texture_manager.h"
#include "asset/asset.h"
#include "handle/handle_pool.h"
#include "mem/mem_diag.h"
#include "log.h"
#include <string.h>

extern engine_context_t g_ctx;

material_param_id material_param(const char *name) {
    unsigned long h = 5381;
    if (!name) return 0;
    for (const unsigned char *p = (const unsigned char*) name; *p; ++p) {
        h = ((h << 5) + h) + (unsigned long) (*p);
    }

    return (material_param_id) h;
}

enum {
    SLOT_CBUFFER_B0 = 0,
    SLOT_TEX_T0 = 0,
    SLOT_SAMP_S0 = 0,
};

typedef struct {
    material_param_id id;
    material_param_type_e type;
    uint8_t slot;
    uint8_t stage;
    uint8_t dirty;

    union {
        float f;
        float vec2[2];
        float vec3[3];
        float vec4[4];
        float mat4[16];
        texture_handle_t tex;
    } data;
} material_param_t;

typedef struct {
    struct rhi_shader *sh;
    struct rhi_pipeline *pl;

    /* Dynamic parameters */
    material_param_t *params;
    uint32_t param_count;
    uint32_t param_capacity;

    /* Constant buffers (auto-managed) */
    struct rhi_buffer *cb_buffers[4]; /* b0~b3 */
    uint8_t *cb_data[4]; /* CPU-side data */
    size_t cb_sizes[4];
    uint8_t cb_dirty[4];

    /* Instance management */
    uint8_t is_instance : 1;  /* If true, don't destroy shader/pipeline */
} material_t;

static handle_pool_t *s_pool = NULL;

static struct rhi_sampler *s_default_sampler = NULL;

/* ===== ��ƿ ===== */
static int ensure_default_sampler(void) {
    if (s_default_sampler) return 1;
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;
    rhi_sampler_desc_t sd = {0};
    sd.min_filter = RHI_FILTER_LINEAR;
    sd.mag_filter = RHI_FILTER_LINEAR;
    sd.mip_bias = 0.0f;
    sd.anisotropy = 1;
    sd.wrap_u = RHI_WRAP_CLAMP;
    sd.wrap_v = RHI_WRAP_CLAMP;
    sd.wrap_w = RHI_WRAP_CLAMP;
    s_default_sampler = rhi->create_sampler(g_ctx.active_device, &sd);
    return s_default_sampler != NULL;
}

int material_system_init(size_t capacity) {
    if (s_pool) return 0;
    if (capacity == 0) capacity = 128;

    s_pool = handle_pool_create(capacity, sizeof(material_t), (size_t) _Alignof(material_t));
    if (!s_pool) {
        MR_LOG(FATAL, "material: handle_pool_create failed");
        return -1;
    }

    if (!ensure_default_sampler()) {
        MR_LOG(ERROR, "material: default sampler create failed");
    }
    return 0;
}

void material_system_shutdown(void) {
    if (!s_pool) return;
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    size_t cap = handle_pool_capacity(s_pool);
    for (uint32_t idx = 0; idx < (uint32_t) cap; ++idx) {
        material_t *m = (material_t*) handle_pool_get(s_pool, make_handle(idx, (uint8_t) 0xFF));
        if (!m) continue;

        /* Free dynamic parameters */
        if (m->params) {
            MARU_FREE(m->params);
            m->params = NULL;
        }

        /* Free constant buffers */
        for (uint32_t i = 0; i < 4; ++i) {
            if (m->cb_buffers[i]) {
                rhi->destroy_buffer(g_ctx.active_device, m->cb_buffers[i]);
                m->cb_buffers[i] = NULL;
            }
            if (m->cb_data[i]) {
                MARU_FREE(m->cb_data[i]);
                m->cb_data[i] = NULL;
            }
        }

        if (m->pl) {
            rhi->destroy_pipeline(g_ctx.active_device, m->pl);
            m->pl = NULL;
        }
        if (m->sh) {
            rhi->destroy_shader(g_ctx.active_device, m->sh);
            m->sh = NULL;
        }
    }

    if (s_default_sampler) {
        g_ctx.active_rhi->destroy_sampler(g_ctx.active_device, s_default_sampler);
        s_default_sampler = NULL;
    }

    handle_pool_destroy(s_pool);
    s_pool = NULL;
}

material_handle_t material_create(const material_desc_t *desc) {
    if (!s_pool || !desc || !desc->shader_path || !desc->vs_entry || !desc->ps_entry) {
        return MAT_HANDLE_INVALID;
    }

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    size_t sz = 0;
    char *blob = asset_read_all(desc->shader_path, &sz, 1);
    if (!blob || sz == 0) {
        MR_LOG(ERROR, "material: shader read failed: %s", desc->shader_path ? desc->shader_path : "(null)");
        if (blob)
            MARU_FREE(blob);
        return MAT_HANDLE_INVALID;
    }

    rhi_shader_desc_t sd;
    memset(&sd, 0, sizeof(sd));
    sd.entry_vs = desc->vs_entry;
    sd.entry_ps = desc->ps_entry;
    sd.blob_vs = blob;
    sd.blob_vs_size = sz;
    sd.blob_ps = blob;
    sd.blob_ps_size = sz;

    rhi_shader_t *sh = rhi->create_shader(g_ctx.active_device, &sd);
    MARU_FREE(blob);
    if (!sh) {
        MR_LOG(ERROR, "material: create_shader failed (%s)", desc->shader_path);
        return MAT_HANDLE_INVALID;
    }

    rhi_pipeline_desc_t pd;
    memset(&pd, 0, sizeof(pd));
    pd.shader = sh;

    /* Default vertex layout: Position(3) + Color(3) + UV(2) */
    static const rhi_vertex_attr_t default_attrs[] = {
        {"POSITION", 0, RHI_VTX_F32x3, 0, 0},
        {"TEXCOORD", 0, RHI_VTX_F32x2, 0, (uint32_t) (sizeof(float) * 3)},
        {"NORMAL", 0, RHI_VTX_F32x2, 0, (uint32_t) (sizeof(float) * 5)},
    };
    pd.layout.attrs = default_attrs;
    pd.layout.attr_count = 3;
    pd.layout.stride[0] = (uint32_t) (sizeof(float) * 8);

    /* Rasterizer state */
    pd.raster.fill = RHI_FILL_SOLID;
    pd.raster.cull = RHI_CULL_BACK;
    pd.raster.front_ccw = 0;
    pd.raster.depth_bias = 0.0f;
    pd.raster.slope_scaled_depth_bias = 0.0f;

    /* Depth-stencil state */
    pd.depthst.depth_test_enable = 1;
    pd.depthst.depth_write_enable = 0;
    pd.depthst.depth_func = RHI_CMP_LEQUAL;

    /* Blend state (alpha blending) */
    pd.blend.enable = true;
    pd.blend.src_rgb = RHI_BLEND_SRC_ALPHA;
    pd.blend.dst_rgb = RHI_BLEND_INV_SRC_ALPHA;
    pd.blend.op_rgb = RHI_BLEND_ADD;
    pd.blend.src_a = RHI_BLEND_SRC_ALPHA;
    pd.blend.dst_a = RHI_BLEND_INV_SRC_ALPHA;
    pd.blend.op_a = RHI_BLEND_ADD;
    pd.blend.write_mask = 0x0F;

    rhi_pipeline_t *pl = rhi->create_pipeline(g_ctx.active_device, &pd);
    if (!pl) {
        rhi->destroy_shader(g_ctx.active_device, sh);
        MR_LOG(ERROR, "material: create_pipeline failed");
        return MAT_HANDLE_INVALID;
    }

    material_t init;
    memset(&init, 0, sizeof(init));
    init.sh = sh;
    init.pl = pl;
    init.params = NULL;
    init.param_count = 0;
    init.param_capacity = 0;
    init.is_instance = 0;  /* Base material */

    handle_t h = handle_pool_alloc(s_pool, &init);
    if (h == HANDLE_INVALID) {
        rhi->destroy_pipeline(g_ctx.active_device, pl);
        rhi->destroy_shader(g_ctx.active_device, sh);
        MR_LOG(ERROR, "material: pool full");
        return MAT_HANDLE_INVALID;
    }

    return (material_handle_t) h;
}

material_handle_t material_create_instance(material_handle_t base) {
    if (!s_pool || base == MAT_HANDLE_INVALID) {
        return MAT_HANDLE_INVALID;
    }

    material_t *base_mat = (material_t*) handle_pool_get(s_pool, (handle_t) base);
    if (!base_mat) {
        MR_LOG(ERROR, "material_create_instance: invalid base material");
        return MAT_HANDLE_INVALID;
    }

    /* Create instance - shares shader/pipeline, clones params */
    material_t instance;
    memset(&instance, 0, sizeof(instance));

    /* Shared resources (pointer copy only) */
    instance.sh = base_mat->sh;
    instance.pl = base_mat->pl;

    /* Per-instance data (deep copy of params array) */
    instance.param_count = base_mat->param_count;
    instance.param_capacity = base_mat->param_capacity;
    instance.is_instance = 1;  /* Mark as instance */

    if (base_mat->param_count > 0 && base_mat->params) {
        size_t params_size = base_mat->param_capacity * sizeof(material_param_t);
        instance.params = (material_param_t*) MARU_MALLOC(params_size);
        if (!instance.params) {
            MR_LOG(ERROR, "material_create_instance: failed to allocate params");
            return MAT_HANDLE_INVALID;
        }
        memcpy(instance.params, base_mat->params, params_size);

        /* Mark all constant buffer slots as dirty for first update */
        for (uint32_t i = 0; i < base_mat->param_count; ++i) {
            material_param_t *p = &instance.params[i];
            if (p->type != MATERIAL_PARAM_TEXTURE) {
                instance.cb_dirty[p->slot] = 1;
            }
        }
    } else {
        instance.params = NULL;
    }

    /* Allocate new handle */
    handle_t h = handle_pool_alloc(s_pool, &instance);
    if (h == HANDLE_INVALID) {
        MR_LOG(ERROR, "material_create_instance: pool full");
        return MAT_HANDLE_INVALID;
    }

    return (material_handle_t) h;
}

void material_destroy(material_handle_t mh) {
    if (!s_pool || mh == MAT_HANDLE_INVALID) return;

    material_t *m = (material_t*) handle_pool_get(s_pool, (handle_t) mh);
    if (!m) return;

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    /* Free per-instance data */
    if (m->params) {
        MARU_FREE(m->params);
        m->params = NULL;
    }

    for (uint32_t i = 0; i < 4; ++i) {
        if (m->cb_buffers[i]) {
            rhi->destroy_buffer(g_ctx.active_device, m->cb_buffers[i]);
            m->cb_buffers[i] = NULL;
        }
        if (m->cb_data[i]) {
            MARU_FREE(m->cb_data[i]);
            m->cb_data[i] = NULL;
        }
    }

    /* Only destroy shared resources if this is NOT an instance */
    if (!m->is_instance) {
        if (m->pl) {
            rhi->destroy_pipeline(g_ctx.active_device, m->pl);
            m->pl = NULL;
        }
        if (m->sh) {
            rhi->destroy_shader(g_ctx.active_device, m->sh);
            m->sh = NULL;
        }
    }

    handle_pool_free(s_pool, (handle_t) mh);
}

static material_param_t *material_find_or_create_param(material_t *m, material_param_id id, material_param_type_e type) {
    for (uint32_t i = 0; i < m->param_count; ++i) {
        if (m->params[i].id == id) {
            if (m->params[i].type != type) {
                MR_LOG(ERROR, "material: param type mismatch");
                return NULL;
            }
            return &m->params[i];
        }
    }

    if (m->param_count >= m->param_capacity) {
        uint32_t new_cap = m->param_capacity ? m->param_capacity * 2 : 8;
        material_param_t *new_params = (material_param_t*) MARU_REALLOC(m->params, new_cap * sizeof(material_param_t));
        if (!new_params) {
            MR_LOG(ERROR, "material: param alloc failed");
            return NULL;
        }
        m->params = new_params;
        m->param_capacity = new_cap;
    }

    material_param_t *p = &m->params[m->param_count++];
    memset(p, 0, sizeof(*p));
    p->id = id;
    p->type = type;
    p->slot = 0; /* Default b0/t0 */
    p->stage = (type == MATERIAL_PARAM_TEXTURE) ? RHI_STAGE_PS : RHI_STAGE_VS;
    p->dirty = 1;
    return p;
}

void material_set_float(material_handle_t mh, const char *name, float value) {
    if (!s_pool || mh == MAT_HANDLE_INVALID || !name) return;
    material_t *m = (material_t*) handle_pool_get(s_pool, (handle_t) mh);
    if (!m) return;

    material_param_id id = material_param(name);
    material_param_t *p = material_find_or_create_param(m, id, MATERIAL_PARAM_FLOAT);
    if (p) {
        p->data.f = value;
        p->dirty = 1;
        m->cb_dirty[p->slot] = 1;
    }
}

void material_set_vec2(material_handle_t mh, const char *name, const float *v) {
    if (!s_pool || mh == MAT_HANDLE_INVALID || !name || !v) return;
    material_t *m = (material_t*) handle_pool_get(s_pool, (handle_t) mh);
    if (!m) return;

    material_param_id id = material_param(name);
    material_param_t *p = material_find_or_create_param(m, id, MATERIAL_PARAM_VEC2);
    if (p) {
        memcpy(p->data.vec2, v, sizeof(float) * 2);
        p->dirty = 1;
        m->cb_dirty[p->slot] = 1;
    }
}

void material_set_vec3(material_handle_t mh, const char *name, const float *v) {
    if (!s_pool || mh == MAT_HANDLE_INVALID || !name || !v) return;
    material_t *m = (material_t*) handle_pool_get(s_pool, (handle_t) mh);
    if (!m) return;

    material_param_id id = material_param(name);
    material_param_t *p = material_find_or_create_param(m, id, MATERIAL_PARAM_VEC3);
    if (p) {
        memcpy(p->data.vec3, v, sizeof(float) * 3);
        p->dirty = 1;
        m->cb_dirty[p->slot] = 1;
    }
}

void material_set_vec4(material_handle_t mh, const char *name, const float *v) {
    if (!s_pool || mh == MAT_HANDLE_INVALID || !name || !v) return;
    material_t *m = (material_t*) handle_pool_get(s_pool, (handle_t) mh);
    if (!m) return;

    material_param_id id = material_param(name);
    material_param_t *p = material_find_or_create_param(m, id, MATERIAL_PARAM_VEC4);
    if (p) {
        memcpy(p->data.vec4, v, sizeof(float) * 4);
        p->dirty = 1;
        m->cb_dirty[p->slot] = 1;
    }
}

void material_set_mat4(material_handle_t mh, const char *name, const float *m16) {
    if (!s_pool || mh == MAT_HANDLE_INVALID || !name || !m16) return;
    material_t *m = (material_t*) handle_pool_get(s_pool, (handle_t) mh);
    if (!m) return;

    material_param_id id = material_param(name);
    material_param_t *p = material_find_or_create_param(m, id, MATERIAL_PARAM_MAT4);
    if (p) {
        memcpy(p->data.mat4, m16, sizeof(float) * 16);
        p->dirty = 1;
        m->cb_dirty[p->slot] = 1;
    }
}

void material_set_texture(material_handle_t mh, const char *name, texture_handle_t tex) {
    if (!s_pool || mh == MAT_HANDLE_INVALID || !name) return;
    material_t *m = (material_t*) handle_pool_get(s_pool, (handle_t) mh);
    if (!m) return;

    material_param_id id = material_param(name);
    material_param_t *p = material_find_or_create_param(m, id, MATERIAL_PARAM_TEXTURE);
    if (p) {
        p->data.tex = tex;
        p->dirty = 1;
    }
}

static void material_update_cbuffers(material_t *m) {
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    for (uint32_t slot = 0; slot < 4; ++slot) {
        if (!m->cb_dirty[slot]) continue;

        size_t offset = 0;
        const size_t max_cb_size = 256 * 16; /* 256 vec4s */

        if (!m->cb_data[slot]) {
            m->cb_data[slot] = (uint8_t*) MARU_MALLOC(max_cb_size);
            if (!m->cb_data[slot]) {
                MR_LOG(ERROR, "material: cb_data alloc failed");
                continue;
            }
            memset(m->cb_data[slot], 0, max_cb_size);
        }

        for (uint32_t i = 0; i < m->param_count; ++i) {
            material_param_t *p = &m->params[i];
            if (p->slot != slot || p->type == MATERIAL_PARAM_TEXTURE) continue;

            size_t size = 0;
            void *src = NULL;

            switch (p->type) {
            case MATERIAL_PARAM_FLOAT: size = 4;
                src = &p->data.f;
                break;
            case MATERIAL_PARAM_VEC2: size = 8;
                src = p->data.vec2;
                break;
            case MATERIAL_PARAM_VEC3: size = 12;
                src = p->data.vec3;
                break;
            case MATERIAL_PARAM_VEC4: size = 16;
                src = p->data.vec4;
                break;
            case MATERIAL_PARAM_MAT4: size = 64;
                src = p->data.mat4;
                break;
            default: continue;
            }

            /* 16-byte alignment */
            offset = (offset + 15) & ~15;
            if (offset + size > max_cb_size) {
                MR_LOG(ERROR, "material: cb overflow");
                break;
            }

            memcpy(m->cb_data[slot] + offset, src, size);
            offset += size;
        }

        if (offset > 0) {
            if (!m->cb_buffers[slot]) {
                rhi_buffer_desc_t bd = {0};
                bd.size = max_cb_size;
                bd.usage = RHI_BUF_CONST;
                m->cb_buffers[slot] = rhi->create_buffer(g_ctx.active_device, &bd, NULL);
                if (!m->cb_buffers[slot]) {
                    MR_LOG(ERROR, "material: cb create failed");
                    continue;
                }
                m->cb_sizes[slot] = max_cb_size;
            }

            rhi->update_buffer(g_ctx.active_device, m->cb_buffers[slot], m->cb_data[slot], offset);
            m->cb_dirty[slot] = 0;
        }
    }
}

void material_bind(rhi_cmd_t *cmd, material_handle_t mh) {
    if (!s_pool || mh == MAT_HANDLE_INVALID || !cmd) return;
    material_t *m = (material_t*) handle_pool_get(s_pool, (handle_t) mh);
    if (!m) return;

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    rhi->cmd_bind_pipeline(cmd, m->pl);

    material_update_cbuffers(m);

    for (uint32_t i = 0; i < m->param_count; ++i) {
        material_param_t *p = &m->params[i];

        if (p->type == MATERIAL_PARAM_TEXTURE) {
            if (p->data.tex != TEX_HANDLE_INVALID) {
                rhi_texture_t *rt = tex_acquire_rhi(p->data.tex);
                if (rt) {
                    rhi->cmd_bind_texture(cmd, rt, p->slot, p->stage);
                }
            }
        } else {
            if (m->cb_buffers[p->slot]) {
                rhi->cmd_bind_const_buffer(cmd, p->slot, m->cb_buffers[p->slot], p->stage);
            }
        }
    }

    if (ensure_default_sampler()) {
        rhi->cmd_bind_sampler(cmd, s_default_sampler, SLOT_SAMP_S0, RHI_STAGE_PS);
    }
}
