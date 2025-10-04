#ifndef MARU_RENDERER_H
#define MARU_RENDERER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations - RHI types hidden */
struct rhi_dispatch;
struct rhi_device;
struct rhi_texture;
struct rhi_render_target;
struct rhi_shader;
struct rhi_pipeline;
struct rhi_sampler;
struct rhi_cmd;

typedef uint32_t material_handle_t;
typedef uint32_t mesh_handle_t;
typedef uint32_t sprite_handle_t;

typedef struct renderer renderer_t;

/* Scene callback - receives renderer instead of cmd */
typedef void (*render_scene_fn)(renderer_t *R, void *user);

struct renderer {
    const struct rhi_dispatch *rhi;
    struct rhi_device *dev;

    int w, h;

    /* Current command (valid only during render) */
    struct rhi_cmd *current_cmd;

    /* Offscreen */
    struct rhi_texture *off_color;
    struct rhi_texture *off_depth;
    struct rhi_render_target *off_rt;

    /* Post */
    struct rhi_shader *post_sh;
    struct rhi_pipeline *post_pl;
    struct rhi_sampler *post_sampler;

    /* Scene callback */
    render_scene_fn scene_cb;
    void *scene_user;
};

int renderer_init(renderer_t *R, const struct rhi_dispatch *rhi, struct rhi_device *dev, int w, int h);
void renderer_resize(renderer_t *R, int w, int h);
void renderer_set_scene(renderer_t *R, render_scene_fn fn, void *user);
void renderer_render(renderer_t *R); /* no present */
void renderer_shutdown(renderer_t *R);

/* Rendering API - hides RHI from user */
void renderer_bind_material(renderer_t *R, material_handle_t mat);
void renderer_draw_mesh(renderer_t *R, mesh_handle_t mesh);
void renderer_draw_sprite(renderer_t *R, sprite_handle_t sprite, float x, float y);

#ifdef __cplusplus
}
#endif
#endif
