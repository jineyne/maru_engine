#include "renderer.h"
#include <string.h>

static const char *s_post_vs =
"struct VSOut{float4 pos:SV_Position;float2 uv:TEXCOORD0;};"
"VSOut main(uint vid : SV_VertexID){"
"  VSOut o;"
"  float2 pos = (vid == 0) ? float2(-1,-1) : (vid == 1) ? float2( 3,-1) : float2(-1, 3);"
"  float2 uv  = (vid == 0) ? float2( 0, 0) : (vid == 1) ? float2( 2, 0) : float2( 0, 2);"
"  o.pos = float4(pos,0,1);"
"  o.uv = float2(uv.x, 1.0 - uv.y);"
"  return o;"
"}";

static const char *s_post_ps =
    "Texture2D gTex:register(t0);SamplerState gSamp:register(s0);"
    "float4 main(float4 pos:SV_Position,float2 uv:TEXCOORD0):SV_Target{"
    "  return gTex.Sample(gSamp, uv); }";

static void destroy_offscreen(renderer_t *R) {
    if (!R || !R->rhi) return;

    const rhi_dispatch_t *r = R->rhi;
    if (R->off_rt) {
        r->destroy_render_target(R->dev, R->off_rt);
        R->off_rt = NULL;
    }

    if (R->off_color) {
        r->destroy_texture(R->dev, R->off_color);
        R->off_color = NULL;
    }

    if (R->off_depth) {
        r->destroy_texture(R->dev, R->off_depth);
        R->off_depth = NULL;
    }
}

static void create_offscreen(renderer_t *R, int w, int h) {
    const rhi_dispatch_t *r = R->rhi;

    rhi_texture_desc_t td = {0};
    td.width = w;
    td.height = h;
    td.mip_levels = 1;
    td.format = RHI_FMT_RGBA8;
    td.usage = RHI_TEX_USAGE_RENDER_TARGET | RHI_TEX_USAGE_SAMPLED;
    R->off_color = r->create_texture(R->dev, &td, NULL);

    rhi_texture_desc_t dd = {0};
    dd.width = w;
    dd.height = h;
    dd.mip_levels = 1;
    dd.format = RHI_FMT_D24S8;
    dd.usage = RHI_TEX_USAGE_DEPTH;
    R->off_depth = r->create_texture(R->dev, &dd, NULL);

    rhi_render_target_desc_t rd = {0};
    rd.color[0].texture = R->off_color;
    rd.color_count = 1;
    rd.depth.texture = R->off_depth;
    R->off_rt = r->create_render_target(R->dev, &rd);
}

static void destroy_post(renderer_t *R) {
    if (!R || !R->rhi) return;
    const rhi_dispatch_t *r = R->rhi;

    if (R->post_pl) {
        r->destroy_pipeline(R->dev, R->post_pl);
        R->post_pl = NULL;
    }

    if (R->post_sh) {
        r->destroy_shader(R->dev, R->post_sh);
        R->post_sh = NULL;
    }

    if (R->post_sampler) {
        r->destroy_sampler(R->dev, R->post_sampler);
        R->post_sampler = NULL;
    }
}

static void create_post(renderer_t *R) {
    const rhi_dispatch_t *r = R->rhi;

    rhi_shader_desc_t sd = {0};
    sd.entry_vs = "main";
    sd.entry_ps = "main";
    sd.blob_vs = s_post_vs;
    sd.blob_vs_size = strlen(s_post_vs);
    sd.blob_ps = s_post_ps;
    sd.blob_ps_size = strlen(s_post_ps);
    R->post_sh = r->create_shader(R->dev, &sd);

    rhi_pipeline_desc_t pd = {0};
    pd.shader = R->post_sh;
    pd.depthst.depth_test_enable = 0;
    pd.depthst.depth_write_enable = 0;
    pd.blend.enable = false;
    R->post_pl = r->create_pipeline(R->dev, &pd);

    rhi_sampler_desc_t samp_desc = {0};
    samp_desc.min_filter = RHI_FILTER_LINEAR;
    samp_desc.mag_filter = RHI_FILTER_LINEAR;
    samp_desc.wrap_u = RHI_WRAP_CLAMP;
    samp_desc.wrap_v = RHI_WRAP_CLAMP;
    samp_desc.wrap_w = RHI_WRAP_CLAMP;
    samp_desc.anisotropy = 1;
    samp_desc.mip_bias = 0.0f;
    R->post_sampler = r->create_sampler(R->dev, &samp_desc);
}

int renderer_init(renderer_t *R, const rhi_dispatch_t *rhi, rhi_device_t *dev, int w, int h) {
    if (!R || !rhi || !dev) return -1;

    memset(R, 0, sizeof(*R));
    R->rhi = rhi;
    R->dev = dev;
    R->w = w;
    R->h = h;
    create_offscreen(R, w, h);
    create_post(R);
    return 0;
}

void renderer_resize(renderer_t *R, int w, int h) {
    if (!R) return;
    if (w <= 0 || h <= 0) return;
    if (R->w == w && R->h == h) return;
    R->w = w;
    R->h = h;

    destroy_offscreen(R);
    create_offscreen(R, w, h);
}

void renderer_set_scene(renderer_t *R, render_scene_fn fn, void *user) {
    if (!R) return;

    R->scene_cb = fn;
    R->scene_user = user;
}

void renderer_render(renderer_t *R) {
    if (!R || !R->rhi) return;
    const rhi_dispatch_t *r = R->rhi;

    rhi_cmd_t *cmd = r->begin_cmd(R->dev);

    /* Pass 1: Offscreen scene */
    const float clear1[4] = {0.1f, 0.1f, 0.1f, 1.0f};
    r->cmd_begin_render(cmd, R->off_rt, clear1);
    if (R->scene_cb) {
        R->scene_cb(cmd, R->scene_user);
    }
    r->cmd_end_render(cmd);

    /* Pass 2: Post to backbuffer */
    rhi_render_target_t *back = r->get_backbuffer_rt ? r->get_backbuffer_rt(R->dev) : NULL;
    r->cmd_begin_render(cmd, back, NULL);
    r->cmd_bind_pipeline(cmd, R->post_pl);

    rhi_binding_t b0[2] = {0};
    b0[0].binding = 0; b0[0].texture = R->off_color;
    r->cmd_bind_set(cmd, &b0[0], 1, RHI_STAGE_PS);

    r->cmd_bind_sampler(cmd, 0, R->post_sampler, RHI_STAGE_PS);

    r->cmd_draw(cmd, 3, 0, 1);
    r->cmd_end_render(cmd);

    r->end_cmd(cmd);
}

void renderer_shutdown(renderer_t *R) {
    if (!R) return;
    destroy_offscreen(R);
    destroy_post(R);
    memset(R, 0, sizeof(*R));
}
