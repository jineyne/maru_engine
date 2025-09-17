#include "rhi_gles.h"

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
    GLuint id;
    GLenum target;
    GLenum internal_fmt;
    int w, h, mips;
    uint32_t usage;
};

struct rhi_shader {
    int linked;
};

struct rhi_pipeline {
    rhi_shader_t *sh;
};

struct rhi_render_target {
    int is_backbuffer; /* 1=default framebuffer(0) */
    GLuint fbo;
    GLuint color_ids[4];
    int color_count;
    GLuint depth_id;
};

struct rhi_cmd {
    int dummy;
};

struct rhi_fence {
    int dummy;
};

#pragma region Helpers

static int gl_srv_conflicts_with_rt(GLuint tex, const rhi_render_target_t *rt) {
    if (!rt) return 0;
    for (int i = 0; i < rt->color_count; ++i) {
        if (rt->color_ids[i] && rt->color_ids[i] == tex) {
            return 1;
        }
    }

    if (rt->depth_id && rt->depth_id == tex) return 1;
    return 0;
}

#pragma endregion Helpers

static rhi_device_t *gles_create_device(const rhi_device_desc_t *d) {
    INFO("creating OpenGL ES device");
    UNUSED(d);
    return (rhi_device_t*)calloc(1, sizeof(rhi_device_t));
}

static void gles_destroy_device(rhi_device_t *d) {
    free(d);
}

static rhi_swapchain_t *gles_get_swapchain(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_swapchain_t*)calloc(1, sizeof(rhi_swapchain_t));
}

static void gles_present(rhi_swapchain_t *s) {
    UNUSED(s);
}

static void gles_resize(rhi_device_t *d, int w, int h) {
    UNUSED(d);
    UNUSED(w);
    UNUSED(h);
}

static rhi_buffer_t *gles_create_buffer(rhi_device_t *d, const rhi_buffer_desc_t *desc, const void *initial) {
    UNUSED(d);
    UNUSED(desc);
    UNUSED(initial);
    return (rhi_buffer_t*)calloc(1, sizeof(rhi_buffer_t));
}

static void gles_destroy_buffer(rhi_device_t *d, rhi_buffer_t *b) {
    UNUSED(d);
    free(b);
}

static rhi_texture_t *gles_create_texture(rhi_device_t *d, const rhi_texture_desc_t *desc, const void *initial) {
    UNUSED(d);
    UNUSED(initial);
    rhi_texture_t *t = (rhi_texture_t*)calloc(1, sizeof(*t));
    glGenTextures(1, &t->id);
    t->target = GL_TEXTURE_2D;
    t->w = desc->width;
    t->h = desc->height;
    t->mips = desc->mip_levels > 0 ? desc->mip_levels : 1;
    t->usage = desc->usage;

    if (desc->usage & RHI_TEX_USAGE_DEPTH) {
        t->internal_fmt = GL_DEPTH24_STENCIL8;
    } else {
        t->internal_fmt = (desc->format == RHI_FMT_BGRA8) ? GL_RGBA8 : GL_RGBA8;
    }

    glBindTexture(t->target, t->id);
    if (desc->usage & RHI_TEX_USAGE_DEPTH) {
        glTexImage2D(t->target, 0, t->internal_fmt, t->w, t->h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    } else {
        glTexImage2D(t->target, 0, t->internal_fmt, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }
    if (t->mips > 1 || (desc->usage & RHI_TEX_USAGE_GEN_MIPS)) {
        glGenerateMipmap(t->target);
    }

    glTexParameteri(t->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(t->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(t->target, 0);
    return t;
}

static void gles_destroy_texture(rhi_device_t *d, rhi_texture_t *t) {
    UNUSED(d);
    if (!t) return;
    if (t->id) {
        glDeleteTextures(1, &t->id);
    }

    free(t);
}

static rhi_shader_t *gles_create_shader(rhi_device_t *d, const rhi_shader_desc_t *sd) {
    UNUSED(d);
    UNUSED(sd);
    return (rhi_shader_t*)calloc(1, sizeof(rhi_shader_t));
}

static void gles_destroy_shader(rhi_device_t *d, rhi_shader_t *s) {
    UNUSED(d);
    free(s);
}

static rhi_pipeline_t *gles_create_pipeline(rhi_device_t *d, const rhi_pipeline_desc_t *pd) {
    UNUSED(d);
    rhi_pipeline_t *p = (rhi_pipeline_t*)calloc(1, sizeof(rhi_pipeline_t));
    p->sh = pd->shader;
    return p;
}

static void gles_destroy_pipeline(rhi_device_t *d, rhi_pipeline_t *p) {
    UNUSED(d);
    free(p);
}

static rhi_render_target_t *gles_create_render_target(rhi_device_t *d, const rhi_render_target_desc_t *desc) {
    UNUSED(d);
    rhi_render_target_t *rt = (rhi_render_target_t*)calloc(1, sizeof(*rt));
    glGenFramebuffers(1, &rt->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

    rt->color_count = desc->color_count;
    for (int i = 0; i < desc->color_count; ++i) {
        rhi_texture_t *tx = desc->color[i].texture;
        rt->color_ids[i] = tx ? tx->id : 0;
        if (tx && !(tx->usage & RHI_TEX_USAGE_RENDER_TARGET)) {
            ERROR("texture %d is not created with RHI_TEX_USAGE_RENDER_TARGET flag, but used as render target");
        }

        if (tx) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, tx->target, tx->id, desc->color[i].mip_level);
        }
    }

    if (desc->depth.texture) {
        rhi_texture_t *dz = desc->depth.texture;
        rt->depth_id = dz->id;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, dz->target, dz->id, desc->depth.mip_level);
    }

    GLenum bufs[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
    glDrawBuffers(rt->color_count > 0 ? rt->color_count : 1, bufs);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &rt->fbo);
        free(rt);
        return NULL;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return rt;
}

static void gles_destroy_render_target(rhi_device_t *d, rhi_render_target_t *rt) {
    UNUSED(d);
    if (!rt) return;
    if (!rt->is_backbuffer && rt->fbo) {
        glDeleteFramebuffers(1, &rt->fbo);
    }

    free(rt);
}

static rhi_render_target_t *gles_get_backbuffer_rt(rhi_device_t *d) {
    UNUSED(d);
    static rhi_render_target_t *s = NULL;
    if (!s) {
        s = (rhi_render_target_t*)calloc(1, sizeof(*s));
        s->is_backbuffer = 1;
    }
    return s;
}

static rhi_texture_t *gles_render_target_get_color_tex(rhi_render_target_t *rt, int index) {
    UNUSED(rt);
    UNUSED(index);
    return NULL;
}

static rhi_cmd_t *gles_begin_cmd(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_cmd_t*)calloc(1, sizeof(rhi_cmd_t));
}

static void gles_end_cmd(rhi_cmd_t *c) { free(c); }

static void gles_cmd_begin_render(rhi_cmd_t *c, rhi_render_target_t *rt, const float clear_rgba[4]) {
    UNUSED(c);
    rhi_render_target_t *use = rt ? rt : gles_get_backbuffer_rt(NULL);
    glBindFramebuffer(GL_FRAMEBUFFER, use->fbo);

    if (clear_rgba) {
        glViewport(0, 0, /* TODO: real size */ 4096, 4096);
        glClearColor(clear_rgba[0], clear_rgba[1], clear_rgba[2], clear_rgba[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
}

static void gles_cmd_end_render(rhi_cmd_t *c) {
    UNUSED(c);
}

static void gles_cmd_bind_pipeline(rhi_cmd_t *c, rhi_pipeline_t *p) {
    UNUSED(c);
    UNUSED(p);
}

static void gles_cmd_bind_set(rhi_cmd_t *c, const rhi_binding_t *binds, int num, uint32_t stages) {
    UNUSED(stages);
    static rhi_render_target_t *s_last_rt = NULL;

    for (int i = 0; i < num; ++i) {
        const rhi_binding_t *b = &binds[i];
        if (!b->texture || !b->texture->id) continue;
        if (gl_srv_conflicts_with_rt(b->texture->id, s_last_rt)) {
            DEBUG_LOG("GL: SRV-RT conflict on unit %u -> skip bind", b->binding);
            continue;
        }

        glActiveTexture(GL_TEXTURE0 + b->binding);
        glBindTexture(b->texture->target, b->texture->id);
    }
}

static void gles_cmd_set_viewport_scissor(rhi_cmd_t *c, int x, int y, int w, int h) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    UNUSED(w);
    UNUSED(h);
}

static void gles_cmd_set_vertex_buffer(rhi_cmd_t *c, int slot, rhi_buffer_t *b) {
    UNUSED(c);
    UNUSED(slot);
    UNUSED(b);
}

static void gles_cmd_set_index_buffer(rhi_cmd_t *c, rhi_buffer_t *b) {
    UNUSED(c);
    UNUSED(b);
}

static void gles_cmd_draw(rhi_cmd_t *c, uint32_t vtx_count, uint32_t first, uint32_t inst) {
    UNUSED(c);
    UNUSED(vtx_count);
    UNUSED(first);
    UNUSED(inst);
}

static void gles_cmd_draw_indexed(rhi_cmd_t *c, uint32_t idx_count, uint32_t first, uint32_t base_vtx, uint32_t inst) {
    UNUSED(c);
    UNUSED(idx_count);
    UNUSED(first);
    UNUSED(base_vtx);
    UNUSED(inst);
}

static rhi_fence_t *gles_fence_create(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_fence_t*)calloc(1, sizeof(rhi_fence_t));
}

static void gles_fence_wait(rhi_fence_t *f) { UNUSED(f); }
static void gles_fence_destroy(rhi_fence_t *f) { free(f); }

PLUGIN_API const rhi_dispatch_t *maru_rhi_entry(void) {
    static const rhi_dispatch_t vtbl = {
        /* device */
        gles_create_device, gles_destroy_device,
        /* swapchain */
        gles_get_swapchain, gles_present,
        gles_resize,
        /* resources */
        gles_create_buffer, gles_destroy_buffer,
        gles_create_texture, gles_destroy_texture,
        gles_create_shader, gles_destroy_shader,
        gles_create_pipeline, gles_destroy_pipeline,
        gles_create_render_target, gles_destroy_render_target,
        gles_get_backbuffer_rt, gles_render_target_get_color_tex,
        /* commands */
        gles_begin_cmd, gles_end_cmd, gles_cmd_begin_render, gles_cmd_end_render,
        gles_cmd_bind_pipeline, gles_cmd_bind_set,
        gles_cmd_set_viewport_scissor, gles_cmd_set_vertex_buffer, gles_cmd_set_index_buffer,
        gles_cmd_draw, gles_cmd_draw_indexed,
        /* sync */
        gles_fence_create, gles_fence_wait, gles_fence_destroy,
    };
    return &vtbl;
}
