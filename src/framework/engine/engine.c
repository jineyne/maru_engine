#include "engine.h"

#include "config.h"
#include "plugin/plugin.h"
#include "rhi/rhi.h"
#include "engine_context.h"

#include <string.h>

#include "math/math.h"
#include "math/proj.h"
#include "platform/window.h"

engine_context_t g_ctx;

static int initialized = 0;
static rhi_swapchain_t *g_swapchain = NULL;
static rhi_render_target_t *g_back_rt = NULL;

static rhi_buffer_t *g_triangle_vb = NULL;
static rhi_shader_t *g_triangle_sh = NULL;
static rhi_pipeline_t *g_triangle_pl = NULL;

static rhi_buffer_t *g_mvp_cb = NULL;

static void update_mvp_from_size(int w, int h) {
    if (!g_ctx.active_rhi || !g_ctx.active_device || !g_mvp_cb) return;

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    rhi_capabilities_t caps;
    rhi->get_capabilities(g_ctx.active_device, &caps);

    if (h <= 0) h = 1;
    float aspect = (float) w / (float) h;

    mat4_t P, V, M, PV, MVP;
    perspective_from_caps(&caps, 60.0f * (3.14159265f / 180.0f), aspect, 0.1f, 100.0f, P);

    vec3_t eye = { 0.f, 0.f, 2.5f };
    vec3_t at = { 0.f, 0.f, 0.f };
    vec3_t up = { 0.f, 1.f, 0.f };
    look_at(eye, at, up, V);

    mat4_identity(M);
    mat4_mul(P, V, PV);
    mat4_mul(PV, M, MVP);

    rhi->update_buffer(g_ctx.active_device, g_mvp_cb, &MVP, sizeof(MVP));
}

static const char *s_vs_hlsl =
    "cbuffer PerFrame : register(b0) { row_major float4x4 uMVP; };"
    "struct VSIn { float3 pos:POSITION; float3 col:COLOR; };"
    "struct VSOut{ float4 pos:SV_Position; float3 col:COLOR; };"
    "VSOut main(VSIn i){ VSOut o; o.pos = mul(float4(i.pos,1), uMVP); o.col=i.col; return o; }";

static const char *s_ps_hlsl =
    "struct PSIn{ float4 pos:SV_Position; float3 col:COLOR; };"
    "float4 main(PSIn i):SV_Target{ return float4(i.col,1); }";

static void create_triangle_resources(void) {
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    float vtx[] = {
        /* top    */ 0.0f, 0.5f, 0.0f, 1, 0, 0,
        /* right  */ 0.5f, -0.5f, 0.0f, 0, 0, 1,
        /* left   */ -0.5f, -0.5f, 0.0f, 0, 1, 0,
    };
    rhi_buffer_desc_t bd = {0};
    bd.size = sizeof(vtx);
    g_triangle_vb = rhi->create_buffer(g_ctx.active_device, &bd, vtx);

    rhi_shader_desc_t sd = {0};
    sd.entry_vs = "main";
    sd.blob_vs = s_vs_hlsl;
    sd.blob_vs_size = strlen(s_vs_hlsl);

    sd.entry_ps = "main";
    sd.blob_ps = s_ps_hlsl;
    sd.blob_ps_size = strlen(s_ps_hlsl);
    g_triangle_sh = rhi->create_shader(g_ctx.active_device, &sd);

    rhi_pipeline_desc_t pd = {0};
    pd.shader = g_triangle_sh;
    g_triangle_pl = rhi->create_pipeline(g_ctx.active_device, &pd);

    mat4_t zero = { 0 };

    rhi_buffer_desc_t cbd = {0};
    cbd.size = sizeof(zero);
    cbd.usage = RHI_BUF_CONST;
    g_mvp_cb = rhi->create_buffer(g_ctx.active_device, &cbd, &zero);

    int cw = 0, ch = 0;
    platform_window_get_size(g_ctx.window, &cw, &ch);
    update_mvp_from_size(cw, ch);
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
    if (initialized) {
        return MARU_OK;
    }

    INFO("maru init");

    maru_config_t cfg = {0};
    if (config_load(config_path, &cfg) != MARU_OK) {
        ERROR("Failed to load config: %s", config_path);
        return MARU_ERR_PARSE;
    }

    engine_context_init(&g_ctx);

    engine_context_load_rhi(&g_ctx, "maru-gl", "gl");
#if defined(_WIN32)
    engine_context_load_rhi(&g_ctx, "maru-dx11", "dx11");
#endif

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

    g_swapchain = g_ctx.active_rhi->get_swapchain(g_ctx.active_device);
    g_back_rt = g_ctx.active_rhi->get_backbuffer_rt(g_ctx.active_device);

    create_triangle_resources();

    config_free(&cfg);

    initialized = 1;
    return MARU_OK;
}

bool maru_engine_tick(void) {
    if (!initialized) return false;
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
    rhi_cmd_t *cmd = rhi->begin_cmd(g_ctx.active_device);

    const float clear[4] = {0.2f, 0.2f, 0.6f, 1.0f};
    rhi->cmd_begin_render(cmd, g_back_rt, clear);

    rhi->cmd_bind_pipeline(cmd, g_triangle_pl);
    rhi->cmd_bind_const_buffer(cmd, 0, g_mvp_cb, RHI_STAGE_VS);
    rhi->cmd_set_vertex_buffer(cmd, 0, g_triangle_vb);
    rhi->cmd_draw(cmd, 3, 0, 1);

    rhi->cmd_end_render(cmd);
    rhi->end_cmd(cmd);

    if (!g_swapchain) g_swapchain = rhi->get_swapchain(g_ctx.active_device);
    rhi->present(g_swapchain);
    return true;
}

void maru_engine_shutdown(void) {
    if (!initialized) return;

    engine_context_shutdown(&g_ctx);

    INFO("maru shutdown");

    initialized = 0;
}
