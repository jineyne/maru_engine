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
    for (const unsigned char *p = (const unsigned char*)name; *p; ++p) {
        h = ((h << 5) + h) + (unsigned long)(*p);
    }

    return (material_param_id)h;
}

enum {
    SLOT_CBUFFER_B0 = 0,
    SLOT_TEX_T0 = 0,
    SLOT_SAMP_S0 = 0,
};

static const material_param_id PID_uMVP = 0;
static const material_param_id PID_gAlbedo = 0;

typedef struct {
    struct rhi_shader *sh;
    struct rhi_pipeline *pl;
    struct rhi_buffer *cb_perframe;

    float uMVP[16];
    int cb_dirty;

    texture_handle_t tex_gAlbedo;
} material_t;

static handle_pool_t *s_pool = NULL;

static struct rhi_sampler *s_default_sampler = NULL;

/* ===== 유틸 ===== */
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

    s_pool = handle_pool_create(capacity, sizeof(material_t), (size_t)_Alignof(material_t));
    if (!s_pool) {
        MR_LOG(FATAL, "material: handle_pool_create failed");
        return -1;
    }

    UNUSED(PID_uMVP);
    UNUSED(PID_gAlbedo);

    if (!ensure_default_sampler()) {
        MR_LOG(ERROR, "material: default sampler create failed");
    }
    return 0;
}

void material_system_shutdown(void) {
    if (!s_pool) return;
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    size_t cap = handle_pool_capacity(s_pool);
    for (uint32_t idx = 0; idx < (uint32_t)cap; ++idx) {
        material_t *m = (material_t*)handle_pool_get(s_pool, make_handle(idx, (uint8_t)0xFF));
        if (!m) continue;

        if (m->cb_perframe) {
            rhi->destroy_buffer(g_ctx.active_device, m->cb_perframe);
            m->cb_perframe = NULL;
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

/* ===== 생성/파괴 ===== */
material_handle_t material_create(const material_desc_t *desc) {
    if (!s_pool || !desc || !desc->shader_path || !desc->vs_entry || !desc->ps_entry) {
        return MAT_HANDLE_INVALID;
    }

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    /* 1) 셰이더 로드 */
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
    rhi_pipeline_t *pl = rhi->create_pipeline(g_ctx.active_device, &pd);
    if (!pl) {
        rhi->destroy_shader(g_ctx.active_device, sh);
        MR_LOG(ERROR, "material: create_pipeline failed");
        return MAT_HANDLE_INVALID;
    }

    rhi_buffer_desc_t bd;
    memset(&bd, 0, sizeof(bd));
    bd.size = sizeof(float) * 16;
    bd.usage = RHI_BUF_CONST;
    rhi_buffer_t *cb = rhi->create_buffer(g_ctx.active_device, &bd, NULL);
    if (!cb) {
        rhi->destroy_pipeline(g_ctx.active_device, pl);
        rhi->destroy_shader(g_ctx.active_device, sh);
        MR_LOG(ERROR, "material: create cbuffer failed");
        return MAT_HANDLE_INVALID;
    }

    material_t init;
    memset(&init, 0, sizeof(init));
    init.sh = sh;
    init.pl = pl;
    init.cb_perframe = cb;
    init.cb_dirty = 1;
    init.tex_gAlbedo = TEX_HANDLE_INVALID;

    handle_t h = handle_pool_alloc(s_pool, &init);
    if (h == HANDLE_INVALID) {
        rhi->destroy_buffer(g_ctx.active_device, cb);
        rhi->destroy_pipeline(g_ctx.active_device, pl);
        rhi->destroy_shader(g_ctx.active_device, sh);
        MR_LOG(ERROR, "material: pool full");
        return MAT_HANDLE_INVALID;
    }

    return (material_handle_t)h;
}

void material_destroy(material_handle_t mh) {
    if (!s_pool || mh == MAT_HANDLE_INVALID) return;

    material_t *m = (material_t*)handle_pool_get(s_pool, (handle_t)mh);
    if (!m) return;

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;
    if (m->cb_perframe) {
        rhi->destroy_buffer(g_ctx.active_device, m->cb_perframe);
        m->cb_perframe = NULL;
    }
    if (m->pl) {
        rhi->destroy_pipeline(g_ctx.active_device, m->pl);
        m->pl = NULL;
    }
    if (m->sh) {
        rhi->destroy_shader(g_ctx.active_device, m->sh);
        m->sh = NULL;
    }

    handle_pool_free(s_pool, (handle_t)mh);
}

int material_set_matrix4(material_handle_t mh, material_param_id id, const float *m16) {
    if (!s_pool || mh == MAT_HANDLE_INVALID || !m16) return 0;
    material_t *m = (material_t*)handle_pool_get(s_pool, (handle_t)mh);
    if (!m) return 0;

    memcpy(m->uMVP, m16, sizeof(float) * 16);
    m->cb_dirty = 1;
    return 1;
}

int material_set_texture(material_handle_t mh, material_param_id id, texture_handle_t tex) {
    if (!s_pool || mh == MAT_HANDLE_INVALID) return 0;
    material_t *m = (material_t*)handle_pool_get(s_pool, (handle_t)mh);
    if (!m) return 0;

    if (id != MATID("gAlbedo")) return 0;
    m->tex_gAlbedo = tex;
    return 1;
}

void renderer_bind_material(rhi_cmd_t *cmd, material_handle_t mh) {
    if (!s_pool || mh == MAT_HANDLE_INVALID || !cmd) return;
    material_t *m = (material_t*)handle_pool_get(s_pool, (handle_t)mh);
    if (!m) return;

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    rhi->cmd_bind_pipeline(cmd, m->pl);

    if (m->cb_dirty && m->cb_perframe) {
        rhi->update_buffer(g_ctx.active_device, m->cb_perframe, m->uMVP, sizeof(m->uMVP));
        m->cb_dirty = 0;
    }
    rhi->cmd_bind_const_buffer(cmd, SLOT_CBUFFER_B0, m->cb_perframe, RHI_STAGE_VS);

    if (!ensure_default_sampler()) {
        // SKIP
    } else {
        rhi->cmd_bind_sampler(cmd, s_default_sampler, SLOT_SAMP_S0, RHI_STAGE_PS);
    }

    if (m->tex_gAlbedo != TEX_HANDLE_INVALID) {
        rhi_texture_t *rt = tex_acquire_rhi(m->tex_gAlbedo);
        if (rt) {
            rhi->cmd_bind_texture(cmd, rt, SLOT_TEX_T0, RHI_STAGE_PS);
        }
    }
}
