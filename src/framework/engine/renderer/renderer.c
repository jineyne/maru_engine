#include "renderer.h"

#include <stdlib.h>
#include <string.h>

#include "rhi/rhi.h"
#include "material/material.h"
#include "asset/mesh.h"
#include "asset/sprite.h"
#include "asset/asset.h"
#include "mem/mem_diag.h"

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

    size_t buf_len;
    char *buf = asset_read_all("shader\\single_fullscreen.hlsl", &buf_len, TRUE);
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

    MARU_FREE(buf);
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

    R->current_cmd = r->begin_cmd(R->dev);

    /* Pass 1: Offscreen scene */
    const float clear1[4] = {0.1f, 0.1f, 0.1f, 1.0f};
    r->cmd_begin_render(R->current_cmd, R->off_rt, clear1);
    if (R->scene_cb) {
        R->scene_cb(R, R->scene_user);
    }
    r->cmd_end_render(R->current_cmd);

    /* Pass 2: Post to backbuffer */
    rhi_render_target_t *back = r->get_backbuffer_rt ? r->get_backbuffer_rt(R->dev) : NULL;
    r->cmd_begin_render(R->current_cmd, back, NULL);
    r->cmd_bind_pipeline(R->current_cmd, R->post_pl);

    rhi_binding_t b0[2] = {0};
    r->cmd_bind_texture(R->current_cmd, R->off_color, 0, RHI_STAGE_PS);
    r->cmd_bind_sampler(R->current_cmd, R->post_sampler, 0, RHI_STAGE_PS);

    r->cmd_draw(R->current_cmd, 3, 0, 1);
    r->cmd_end_render(R->current_cmd);

    r->end_cmd(R->current_cmd);
    R->current_cmd = NULL;
}

void renderer_shutdown(renderer_t *R) {
    if (!R) return;
    destroy_offscreen(R);
    destroy_post(R);
    memset(R, 0, sizeof(*R));
}

/* Rendering API */
void renderer_bind_material(renderer_t *R, material_handle_t mat) {
    if (!R || !R->current_cmd) return;
    material_bind(R->current_cmd, mat);
}

void renderer_draw_mesh(renderer_t *R, mesh_handle_t mesh) {
    if (!R || !R->current_cmd) return;
    mesh_bind(R->current_cmd, mesh);
    mesh_draw(R->current_cmd, mesh);
}

void renderer_draw_sprite(renderer_t *R, sprite_handle_t sprite, float x, float y) {
    if (!R || !R->current_cmd) return;
    sprite_draw(R->current_cmd, sprite, x, y);
}
