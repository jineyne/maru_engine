#ifndef MARU_RENDERER_H
#define MARU_RENDERER_H

#include "rhi/rhi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*render_scene_fn)(rhi_cmd_t *cmd, void *user);

typedef struct renderer {
    const rhi_dispatch_t *rhi;
    rhi_device_t *dev;

    int w, h;

    /* Offscreen */
    rhi_texture_t *off_color;
    rhi_texture_t *off_depth;
    rhi_render_target_t *off_rt;

    /* Post */
    rhi_shader_t *post_sh;
    rhi_pipeline_t *post_pl;

    /* Scene callback */
    render_scene_fn scene_cb;
    void *scene_user;
} renderer_t;

int renderer_init(renderer_t *R, const rhi_dispatch_t *rhi, rhi_device_t *dev, int w, int h);
void renderer_resize(renderer_t *R, int w, int h);
void renderer_set_scene(renderer_t *R, render_scene_fn fn, void *user);
void renderer_render(renderer_t *R); /* no present */
void renderer_shutdown(renderer_t *R);

#ifdef __cplusplus
}
#endif
#endif
