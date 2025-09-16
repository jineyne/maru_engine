#include "rhi_gl.h"

#include <stdlib.h>

struct rhi_device {
    int dummy;
};

struct rhi_swapchain {
    int dummy;
};

struct rhi_buffer {
    size_t size;
};

struct rhi_texture {
    int w, h;
};

struct rhi_shader {
    int linked;
};

struct rhi_pipeline {
    struct rhi_shader_t *sh;
};

struct rhi_cmd {
    int dummy;
};

struct rhi_fence {
    int dummy;
};

static rhi_device_t *gl_create_device(const rhi_device_desc_t *d) {
    INFO("creating openGL device");

    UNUSED(d);
    return (rhi_device_t*)calloc(1, sizeof(rhi_device_t));
}

static void gl_destroy_device(rhi_device_t *d) { free(d); }

static rhi_swapchain_t *gl_get_swapchain(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_swapchain_t*)calloc(1, sizeof(rhi_swapchain_t));
}

static void gl_present(rhi_swapchain_t *s) {
    UNUSED(s); /* swap buffers here */
}

static void gl_resize(rhi_device_t* d, int w, int h) {
    UNUSED(d); UNUSED(w); UNUSED(h);
}

static rhi_buffer_t *gl_create_buffer(rhi_device_t *d, const rhi_buffer_desc_t *desc, const void *initial) {
    UNUSED(d);
    UNUSED(initial);
    return (rhi_buffer_t*)calloc(1, sizeof(rhi_buffer_t));
}

static void gl_destroy_buffer(rhi_device_t *d, rhi_buffer_t *b) {
    UNUSED(d);
    free(b);
}

static rhi_texture_t *gl_create_texture(rhi_device_t *d, const rhi_texture_desc_t *desc, const void *initial) {
    UNUSED(d);
    UNUSED(initial);
    return (rhi_texture_t*)calloc(1, sizeof(rhi_texture_t));
}

static void gl_destroy_texture(rhi_device_t *d, rhi_texture_t *t) {
    UNUSED(d);
    free(t);
}

static rhi_shader_t *gl_create_shader(rhi_device_t *d, const rhi_shader_desc_t *sd) {
    UNUSED(d);
    UNUSED(sd);
    return (rhi_shader_t*)calloc(1, sizeof(rhi_shader_t));
}

static void gl_destroy_shader(rhi_device_t *d, rhi_shader_t *s) {
    UNUSED(d);
    free(s);
}

static rhi_pipeline_t *gl_create_pipeline(rhi_device_t *d, const rhi_pipeline_desc_t *pd) {
    UNUSED(d);
    rhi_pipeline_t *p = (rhi_pipeline_t*)calloc(1, sizeof(rhi_pipeline_t));
    p->sh = pd->shader;
    return p;
}

static void gl_destroy_pipeline(rhi_device_t *d, rhi_pipeline_t *p) {
    UNUSED(d);
    free(p);
}

static rhi_cmd_t *gl_begin_cmd(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_cmd_t*)calloc(1, sizeof(rhi_cmd_t));
}

static void gl_end_cmd(rhi_cmd_t *c) { free(c); }

static void gl_cmd_begin_render(rhi_cmd_t *c, rhi_texture_t *color, rhi_texture_t *depth, const float clear[4]) {
    UNUSED(c);
    UNUSED(color);
    UNUSED(depth);
    UNUSED(clear);
}

static void gl_cmd_end_render(rhi_cmd_t *c) { (void)c; }

static void gl_cmd_bind_pipeline(rhi_cmd_t *c, rhi_pipeline_t *p) {
    UNUSED(c);
    UNUSED(p);
}

static void gl_cmd_bind_set(rhi_cmd_t *c, const rhi_binding_t *binds, int num, uint32_t stages) {
    UNUSED(c);
    UNUSED(binds);
    UNUSED(num);
    UNUSED(stages);
}

static void gl_cmd_set_viewport_scissor(rhi_cmd_t *c, int x, int y, int w, int h) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    UNUSED(w);
    UNUSED(h);
}

static void gl_cmd_set_vertex_buffer(rhi_cmd_t *c, int slot, rhi_buffer_t *b) {
    UNUSED(c);
    UNUSED(slot);
    UNUSED(b);
}

static void gl_cmd_set_index_buffer(rhi_cmd_t *c, rhi_buffer_t *b) {
    UNUSED(c);
    UNUSED(b);
}

static void gl_cmd_draw(rhi_cmd_t *c, uint32_t vtx_count, uint32_t first, uint32_t inst) {
    UNUSED(c);
    UNUSED(vtx_count);
    UNUSED(first);
    UNUSED(inst);
}

static void gl_cmd_draw_indexed(rhi_cmd_t *c, uint32_t idx_count, uint32_t first, uint32_t base_vtx, uint32_t inst) {
    UNUSED(c);
    UNUSED(idx_count);
    UNUSED(first);
    UNUSED(base_vtx);
    UNUSED(inst);
}

static rhi_fence_t *gl_fence_create(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_fence_t*)calloc(1, sizeof(rhi_fence_t));
}

static void gl_fence_wait(rhi_fence_t *f) { (void)f; }
static void gl_fence_destroy(rhi_fence_t *f) { free(f); }

PLUGIN_API const rhi_dispatch_t *maru_rhi_entry(void) {
    static const rhi_dispatch_t vtbl = {
        /* device */
        gl_create_device, gl_destroy_device,
        /* swapchain */
        gl_get_swapchain, gl_present,
        gl_resize,
        /* resources */
        gl_create_buffer, gl_destroy_buffer,
        gl_create_texture, gl_destroy_texture,
        gl_create_shader, gl_destroy_shader,
        gl_create_pipeline, gl_destroy_pipeline,
        /* commands */
        gl_begin_cmd, gl_end_cmd, gl_cmd_begin_render, gl_cmd_end_render,
        gl_cmd_bind_pipeline, gl_cmd_bind_set,
        gl_cmd_set_viewport_scissor, gl_cmd_set_vertex_buffer, gl_cmd_set_index_buffer,
        gl_cmd_draw, gl_cmd_draw_indexed,
        /* sync */
        gl_fence_create, gl_fence_wait, gl_fence_destroy,
    };
    return &vtbl; /* immutable vtable */
}
