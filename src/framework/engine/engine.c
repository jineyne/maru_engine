#include "engine.h"

#include "engine_context.h"

#include "config.h"
#include "rhi/rhi.h"
#include "renderer/renderer.h"

#include <string.h>

#include "asset/asset.h"
#include "asset/texture_manager.h"
#include "math/math.h"
#include "math/proj.h"
#include "platform/window.h"

#include "time.h"
#include "mem/mem_diag.h"
#include "mem/mem_frame.h"

typedef struct boot_prof_s {
    uint64_t t0;
    uint64_t last;
} boot_prof_t;

static inline boot_prof_t boot_prof_begin(void) {
    uint64_t now = time_now_ms();
    return (boot_prof_t){.t0 = now, .last = now};
}

static inline void boot_prof_step(boot_prof_t *p, const char *tag) {
    uint64_t now = time_now_ms();
    INFO("[boot] %-22s : %4llu ms", tag, (unsigned long long)(now - p->last));
    p->last = now;
}

static inline void boot_prof_total(boot_prof_t *p) {
    uint64_t now = time_now_ms();
    INFO("[boot] %-22s : %4llu ms", "TOTAL INIT", (unsigned long long)(now - p->t0));
}

engine_context_t g_ctx;

static int initialized = 0;

static renderer_t g_renderer;

static rhi_swapchain_t *g_swapchain = NULL;
static rhi_render_target_t *g_back_rt = NULL;

static rhi_buffer_t *g_triangle_vb = NULL;
static rhi_buffer_t *g_triangle_ib = NULL;
static rhi_shader_t *g_triangle_sh = NULL;
static rhi_pipeline_t *g_triangle_pl = NULL;

static rhi_buffer_t *g_mvp_cb = NULL;

static texture_handle_t g_texture = NULL;
static rhi_sampler_t *g_sampler = NULL;

static void update_mvp_from_size(int w, int h) {
    if (!g_ctx.active_rhi || !g_ctx.active_device || !g_mvp_cb) return;
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    static float s_angle = 0.0f;
    s_angle += 0.8f * (PI / 180.0f);

    rhi_capabilities_t caps;
    rhi->get_capabilities(g_ctx.active_device, &caps);

    if (h <= 0) h = 1;
    float aspect = (float) w / (float) h;

    mat4_t P, V, M, R, PV, MVP;
    perspective_from_caps(&caps, 60.0f * (PI / 180.0f), aspect, 0.1f, 100.0f, P);

    vec3_t eye = {0.f, 0.f, 2.5f};
    vec3_t at = {0.f, 0.f, 0.f};
    vec3_t up = {0.f, 1.f, 0.f};
    look_at(eye, at, up, V);

    mat4_identity(M);
    glm_rotate_y(M, s_angle, R);
    mat4_mul(P, V, PV);
    mat4_mul(PV, R, MVP);
    mat4_to_backend_order(&caps, MVP, MVP);

    rhi->update_buffer(g_ctx.active_device, g_mvp_cb, &MVP, sizeof(MVP));
}

static void record_scene(rhi_cmd_t *cmd, void *user) {
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    rhi->cmd_bind_pipeline(cmd, g_triangle_pl);
    rhi->cmd_bind_const_buffer(cmd, 0, g_mvp_cb, RHI_STAGE_VS);

    rhi_texture_t *rt = tex_acquire_rhi(g_texture);
    if (rt != NULL) {
        rhi->cmd_bind_texture(cmd, rt, 0, RHI_STAGE_PS);
    }
    rhi->cmd_bind_sampler(cmd, g_sampler, 0, RHI_STAGE_PS);

    rhi->cmd_set_vertex_buffer(cmd, 0, g_triangle_vb);
    rhi->cmd_set_index_buffer(cmd, g_triangle_ib);
    rhi->cmd_draw_indexed(cmd, 3, 0, 0, 1);
}

static void create_triangle_resources(void) {
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    float vtx[] = {
        //          position             color     uv
        /* top   */ 0.0f, 0.5f, 0.0f, 1, 0, 0, 0.5f, 0.0f,
        /* right */ 0.5f, -0.5f, 0.0f, 0, 0, 1, 1.0f, 1.0f,
        /* left  */ -0.5f, -0.5f, 0.0f, 0, 1, 0, 0.0f, 1.0f,
    };
    rhi_buffer_desc_t bd = {0};
    bd.size = sizeof(vtx);
    bd.usage = RHI_BUF_VERTEX;
    bd.stride = sizeof(float) * 8;
    g_triangle_vb = rhi->create_buffer(g_ctx.active_device, &bd, vtx);

    uint32_t idx[] = {0, 1, 2};
    rhi_buffer_desc_t ibd = {0};
    ibd.size = sizeof(idx);
    ibd.usage = RHI_BUF_INDEX;
    g_triangle_ib = rhi->create_buffer(g_ctx.active_device, &ibd, idx);

    size_t buf_len = 0;
    char *buf = asset_read_all("shader\\default.hlsl", &buf_len, TRUE);
    if (buf == NULL) {
        FATAL("unable to load shader");
        return;
    }

    rhi_shader_desc_t sd = {0};
    sd.entry_vs = "VSMain";
    sd.blob_vs = buf;
    sd.blob_vs_size = buf_len;

    sd.entry_ps = "PSMain";
    sd.blob_ps = buf;
    sd.blob_ps_size = buf_len;
    g_triangle_sh = rhi->create_shader(g_ctx.active_device, &sd);

    rhi_pipeline_desc_t pd = (rhi_pipeline_desc_t){0};
    pd.shader = g_triangle_sh;

    static const rhi_vertex_attr_t attrs[] = {
        {"POSITION", 0, RHI_VTX_F32x3, 0, 0},
        {"COLOR", 0, RHI_VTX_F32x3, 0, (uint32_t) (sizeof(float) * 3)},
        {"TEXCOORD", 0, RHI_VTX_F32x2, 0, (uint32_t) (sizeof(float) * 6)},
    };
    pd.layout.attrs = attrs;
    pd.layout.attr_count = 3;
    pd.layout.stride[0] = (uint32_t) (sizeof(float) * 8);

    pd.raster.fill = RHI_FILL_SOLID;
    pd.raster.cull = RHI_CULL_BACK;
    pd.raster.front_ccw = 0;
    pd.raster.depth_bias = 0.0f;
    pd.raster.slope_scaled_depth_bias = 0.0f;

    pd.depthst.depth_test_enable = 1;
    pd.depthst.depth_write_enable = 0;
    pd.depthst.depth_func = RHI_CMP_LEQUAL;

    pd.blend.enable = true;
    pd.blend.src_rgb = RHI_BLEND_SRC_ALPHA;
    pd.blend.dst_rgb = RHI_BLEND_INV_SRC_ALPHA;
    pd.blend.op_rgb = RHI_BLEND_ADD;
    pd.blend.src_a = RHI_BLEND_SRC_ALPHA;
    pd.blend.dst_a = RHI_BLEND_INV_SRC_ALPHA;
    pd.blend.op_a = RHI_BLEND_ADD;
    pd.blend.write_mask = 0x0F;

    g_triangle_pl = rhi->create_pipeline(g_ctx.active_device, &pd);

    mat4_t zero = {0};

    rhi_buffer_desc_t cbd = {0};
    cbd.size = sizeof(zero);
    cbd.usage = RHI_BUF_CONST;
    g_mvp_cb = rhi->create_buffer(g_ctx.active_device, &cbd, &zero);

    int cw = 0, ch = 0;
    platform_window_get_size(g_ctx.window, &cw, &ch);
    update_mvp_from_size(cw, ch);

    asset_texture_opts_t opts = {
        .gen_mips = 1,
        .flip_y = 0,
        .force_rgba = 1
    };
    g_texture = tex_create_from_file("texture\\karina.jpg", &opts);

    rhi_sampler_desc_t sampler_desc = {0};
    sampler_desc.min_filter = RHI_FILTER_LINEAR;
    sampler_desc.mag_filter = RHI_FILTER_LINEAR;
    sampler_desc.mip_bias = 0.0f;
    sampler_desc.wrap_u = RHI_WRAP_CLAMP;
    sampler_desc.wrap_v = RHI_WRAP_CLAMP;
    sampler_desc.wrap_w = RHI_WRAP_CLAMP;
    g_sampler = g_ctx.active_rhi->create_sampler(g_ctx.active_device, &sampler_desc);

    MARU_FREE(buf);
}

static const char *map_backend_to_regname(const char *backend) {
    if (!backend) return "gl";
    if (strcmp(backend, "dx") == 0 || strcmp(backend, "dx11") == 0) return "dx11";
    if (strcmp(backend, "gles") == 0) return "gles";
    return "gl";
}

static const rhi_backend map_backend_to_backend_key(const char *backend) {
    if (!backend) return RHI_BACKEND_GL;
    if (strcmp(backend, "dx") == 0 || strcmp(backend, "dx11") == 0) return RHI_BACKEND_DX11;
    if (strcmp(backend, "gles") == 0) return RHI_BACKEND_GLES;
    return RHI_BACKEND_GL;
}

int maru_engine_init(const char *config_path) {
    boot_prof_t prof = boot_prof_begin();

    if (initialized) {
        return MARU_OK;
    }

    INFO("maru init");

    asset_init(NULL);
    boot_prof_step(&prof, "asset_init");

    maru_config_t cfg = {0};
    if (config_load(config_path, &cfg) != MARU_OK) {
        ERROR("Failed to load config: %s", config_path);
        return MARU_ERR_PARSE;
    }

    boot_prof_step(&prof, "config_load");

    engine_context_init(&g_ctx);
    boot_prof_step(&prof, "engine_context_init");

    engine_context_load_rhi(&g_ctx, "maru-gl", "gl");
#if defined(_WIN32)
    engine_context_load_rhi(&g_ctx, "maru-dx11", "dx11");
#endif
    boot_prof_step(&prof, "register_rhi_backends");

    const char *want = map_backend_to_regname(cfg.graphics_backend);

    int win_w = (cfg.gfx_width > 0) ? cfg.gfx_width : 1280;
    int win_h = (cfg.gfx_height > 0) ? cfg.gfx_height : 720;
    int win_vsync = (cfg.gfx_vsync != 0);

    g_ctx.window = platform_window_create(NULL, win_w, win_h, win_vsync);
    if (!g_ctx.window) {
        ERROR("Failed to create platform window");
        engine_context_shutdown(&g_ctx);
        return MARU_ERR_INVALID;
    }
    boot_prof_step(&prof, "window_create");

    rhi_device_desc_t device_desc = (rhi_device_desc_t){0};
    device_desc.backend = map_backend_to_backend_key(want);
    device_desc.native_window = g_ctx.window->handle;
    device_desc.width = win_w;
    device_desc.height = win_h;
    device_desc.vsync = win_vsync;

    if (engine_context_select_rhi(&g_ctx, want, &device_desc) != MARU_OK) {
        if (strcmp(want, "gl") != 0 && engine_context_select_rhi(&g_ctx, "gl", &device_desc) == MARU_OK) {
            INFO("RHI fallback to gl");
        } else if (strcmp(want, "gles") != 0 && engine_context_select_rhi(&g_ctx, "gles", &device_desc) == MARU_OK) {
            INFO("RHI fallback to gles");
        } else {
            ERROR("no usable RHI backend (wanted: %s)", want);
            engine_context_shutdown(&g_ctx);
            return MARU_ERR_INVALID;
        }
    }
    boot_prof_step(&prof, "rhi_select+device_create");

    g_swapchain = g_ctx.active_rhi->get_swapchain(g_ctx.active_device);
    g_back_rt = g_ctx.active_rhi->get_backbuffer_rt(g_ctx.active_device);
    boot_prof_step(&prof, "swapchain+backbuffer");

    frame_arena_init(8 * 1024 * 1024, 2);

    texture_manager_init(512);

    create_triangle_resources();
    boot_prof_step(&prof, "create_triangle_resources");

    int cw = 0, ch = 0;
    platform_window_get_size(g_ctx.window, &cw, &ch);
    renderer_init(&g_renderer, g_ctx.active_rhi, g_ctx.active_device, cw, ch);
    boot_prof_step(&prof, "renderer_init");

    renderer_set_scene(&g_renderer, record_scene, NULL);
    boot_prof_step(&prof, "renderer_set_scene");

    config_free(&cfg);
    boot_prof_total(&prof);


    initialized = 1;
    return MARU_OK;
}

static size_t frame_count = 0;
bool maru_engine_tick(void) {
    if (!initialized) return false;

    frame_arena_begin(frame_count++);

    if (platform_should_close(g_ctx.window)) return false;
    platform_poll_events();

    int cur_w = 0, cur_h = 0;
    platform_window_get_size(g_ctx.window, &cur_w, &cur_h);
    if (g_ctx.active_rhi && g_ctx.active_rhi->resize) {
        g_ctx.active_rhi->resize(g_ctx.active_device, cur_w, cur_h);

        if (g_ctx.active_rhi->get_backbuffer_rt) {
            rhi_render_target_t *new_rt = g_ctx.active_rhi->get_backbuffer_rt(g_ctx.active_device);
            if (new_rt) {
                g_back_rt = new_rt;
            }
        }
    }

    update_mvp_from_size(cur_w, cur_h);

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    renderer_resize(&g_renderer, cur_w, cur_h);
    renderer_render(&g_renderer);

    if (!g_swapchain) g_swapchain = rhi->get_swapchain(g_ctx.active_device);
    rhi->present(g_swapchain);
    return true;
}

void maru_engine_shutdown(void) {
    if (!initialized) return;

    tex_destroy(g_texture);

    texture_manager_shutdown();

    frame_arena_shutdown();

    if (g_ctx.active_rhi) {
        const rhi_dispatch_t *rhi = g_ctx.active_rhi;

        if (g_sampler) {
            rhi->destroy_sampler(g_ctx.active_device, g_sampler);
            g_sampler = NULL;
        }
        if (g_triangle_pl) {
            rhi->destroy_pipeline(g_ctx.active_device, g_triangle_pl);
            g_triangle_pl = NULL;
        }
        if (g_triangle_sh) {
            rhi->destroy_shader(g_ctx.active_device, g_triangle_sh);
            g_triangle_sh = NULL;
        }
        if (g_mvp_cb) {
            rhi->destroy_buffer(g_ctx.active_device, g_mvp_cb);
            g_mvp_cb = NULL;
        }
        if (g_triangle_ib) {
            rhi->destroy_buffer(g_ctx.active_device, g_triangle_ib);
            g_triangle_ib = NULL;
        }
        if (g_triangle_vb) {
            rhi->destroy_buffer(g_ctx.active_device, g_triangle_vb);
            g_triangle_vb = NULL;
        }
    }

    renderer_shutdown(&g_renderer);

    engine_context_shutdown(&g_ctx);

    INFO("maru shutdown");

    initialized = 0;
}
