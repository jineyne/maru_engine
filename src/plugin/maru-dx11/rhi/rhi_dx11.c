#include "rhi_dx11.h"

#include <stdlib.h>

#include "export.h"

#define COBJMACROS
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>

typedef struct dx11_state {
    HWND hwnd;
    int w, h, vsync;

    IDXGISwapChain *sc;
    ID3D11Device *dev;
    ID3D11DeviceContext *ctx;
    ID3D11RenderTargetView *rtv;
} dx11_state_t;

struct rhi_device {
    dx11_state_t *st;
};

struct rhi_swapchain {
    dx11_state_t *st;
};

typedef struct {
    ID3D11Buffer *buf;
    UINT stride;
    UINT offset;
    size_t size;
} dx11_buf;

struct rhi_buffer {
    dx11_buf vb;
};

struct rhi_texture {
    int w, h;
};

typedef struct {
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3DBlob *vs_blob; /* for input layout */
} dx11_shader;

struct rhi_shader {
    dx11_shader sh;
};

typedef struct {
    rhi_shader_t *sh;
    ID3D11InputLayout *il;
} dx11_pipeline;

struct rhi_pipeline {
    dx11_pipeline p;
};

struct rhi_cmd {
    dx11_state_t *st;
};

struct rhi_fence {
    int dummy;
};

#pragma region helpers

static void dx11_release_all(dx11_state_t *st) {
    if (!st) return;
    if (st->rtv) {
        ID3D11RenderTargetView_Release(st->rtv);
        st->rtv = NULL;
    }
    if (st->ctx) {
        ID3D11DeviceContext_Release(st->ctx);
        st->ctx = NULL;
    }
    if (st->dev) {
        ID3D11Device_Release(st->dev);
        st->dev = NULL;
    }
    if (st->sc) {
        IDXGISwapChain_Release(st->sc);
        st->sc = NULL;
    }
}

static int dx11_create_backbuffer_rtv(dx11_state_t *st) {
    ID3D11Texture2D *backbuf = NULL;
    HRESULT hr = IDXGISwapChain_GetBuffer(st->sc, 0, &IID_ID3D11Texture2D, (void**)&backbuf);
    if (FAILED(hr)) return -1;

    hr = ID3D11Device_CreateRenderTargetView(st->dev, (ID3D11Resource*)backbuf, NULL, &st->rtv);
    ID3D11Texture2D_Release(backbuf);
    return FAILED(hr) ? -1 : 0;
}

#pragma endregion helpers

static rhi_device_t *dx11_create_device(const rhi_device_desc_t *desc) {
    if (!desc || !desc->native_window) return NULL;

    dx11_state_t *st = (dx11_state_t*)calloc(1, sizeof(dx11_state_t));
    if (!st) return NULL;

    st->hwnd = (HWND)desc->native_window;
    st->w = (desc->width > 0) ? desc->width : 1280;
    st->h = (desc->height > 0) ? desc->height : 720;
    st->vsync = desc->vsync ? 1 : 0;

    DXGI_SWAP_CHAIN_DESC sd;
    memset(&sd, 0, sizeof(sd));
    sd.BufferDesc.Width = st->w;
    sd.BufferDesc.Height = st->h;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.OutputWindow = st->hwnd;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL fls[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL got = 0;

    UINT flags = 0;
#if !defined(NDEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags,
                                               fls, (UINT)(sizeof(fls) / sizeof(fls[0])),
                                               D3D11_SDK_VERSION,
                                               &sd, &st->sc, &st->dev, &got, &st->ctx
    );
    if (FAILED(hr)) {
        dx11_release_all(st);
        free(st);
        return NULL;
    }

    if (dx11_create_backbuffer_rtv(st) != 0) {
        dx11_release_all(st);
        free(st);
        return NULL;
    }

    rhi_device_t *dev = (rhi_device_t*)calloc(1, sizeof(rhi_device_t));
    if (!dev) {
        dx11_release_all(st);
        free(st);
        return NULL;
    }
    dev->st = st;

    return dev;
}

static void dx11_destroy_device(rhi_device_t *d) {
    if (!d) return;

    dx11_state_t *st = d->st;
    dx11_release_all(st);
    free(st);

    free(d);
}

static rhi_swapchain_t *dx11_get_swapchain(rhi_device_t *desc) {
    if (!desc || !desc->st) return NULL;
    rhi_swapchain_t *s = (rhi_swapchain_t*)calloc(1, sizeof(rhi_swapchain_t));
    if (!s) return NULL;
    s->st = desc->st;
    return s;
}

static void dx11_present(rhi_swapchain_t *s) {
    if (!s || !s->st || !s->st->sc) return;
    IDXGISwapChain_Present(s->st->sc, s->st->vsync ? 1 : 0, 0);
}

static rhi_buffer_t *dx11_create_buffer(rhi_device_t *d, const rhi_buffer_desc_t *desc, const void *initial) {
    if (!d || !d->st || !desc || desc->size == 0) return NULL;

    D3D11_BUFFER_DESC bd = {0};
    bd.ByteWidth = (UINT)desc->size;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; /* 데모: VB로 사용 */

    D3D11_SUBRESOURCE_DATA srd = {0};
    srd.pSysMem = initial;

    ID3D11Buffer *buf = NULL;
    HRESULT hr = ID3D11Device_CreateBuffer(d->st->dev, &bd, initial ? &srd : NULL, &buf);
    if (FAILED(hr)) return NULL;

    rhi_buffer_t *b = (rhi_buffer_t*)calloc(1, sizeof(rhi_buffer_t));
    b->vb.buf = buf;
    b->vb.size = desc->size;
    return b;
}

static void dx11_destroy_buffer(rhi_device_t *d, rhi_buffer_t *b) {
    UNUSED(d);

    if (!b) return;

    if (b->vb.buf) {
        ID3D11Buffer_Release(b->vb.buf);
    }
    free(b);
}

static rhi_texture_t *dx11_create_texture(rhi_device_t *d, const rhi_texture_desc_t *desc, const void *initial) {
    UNUSED(d);
    UNUSED(desc);
    UNUSED(initial);
    return (rhi_texture_t*)calloc(1, sizeof(rhi_texture_t));
}

static void dx11_destroy_texture(rhi_device_t *d, rhi_texture_t *t) {
    UNUSED(d);
    free(t);
}

static rhi_shader_t *dx11_create_shader(rhi_device_t *d, const rhi_shader_desc_t *sd) {
    if (!d || !sd || !sd->entry_vs || !sd->entry_ps) return NULL;

    ID3DBlob *vsb = NULL, *psb = NULL, *err = NULL;
    HRESULT hr = D3DCompile(sd->blob_vs, sd->blob_vs_size, "vs",
                            NULL, NULL, sd->entry_vs, "vs_5_0", 0, 0, &vsb, &err);
    if (FAILED(hr)) {
        if (err)
            ID3D10Blob_Release(err);
        return NULL;
    }
    hr = D3DCompile(sd->blob_ps, sd->blob_ps_size, "ps",
                    NULL, NULL, sd->entry_ps, "ps_5_0", 0, 0, &psb, &err);
    if (FAILED(hr)) {
        ID3D10Blob_Release(vsb);
        if (err)
            ID3D10Blob_Release(err);
        return NULL;
    }

    ID3D11VertexShader *vs = NULL;
    ID3D11PixelShader *ps = NULL;
    hr = ID3D11Device_CreateVertexShader(d->st->dev, ID3D10Blob_GetBufferPointer(vsb),
                                         (SIZE_T)ID3D10Blob_GetBufferSize(vsb), NULL, &vs);
    if (FAILED(hr)) {
        ID3D10Blob_Release(vsb);
        ID3D10Blob_Release(psb);
        return NULL;
    }
    hr = ID3D11Device_CreatePixelShader(d->st->dev, ID3D10Blob_GetBufferPointer(psb),
                                        (SIZE_T)ID3D10Blob_GetBufferSize(psb), NULL, &ps);
    if (FAILED(hr)) {
        ID3D11VertexShader_Release(vs);
        ID3D10Blob_Release(vsb);
        ID3D10Blob_Release(psb);
        return NULL;
    }

    rhi_shader_t *sh = (rhi_shader_t*)calloc(1, sizeof(rhi_shader_t));
    sh->sh.vs = vs;
    sh->sh.ps = ps;
    sh->sh.vs_blob = vsb;

    ID3D10Blob_Release(psb);
    return sh;
}

static void dx11_destroy_shader(rhi_device_t *d, rhi_shader_t *s) {
    UNUSED(d);

    if (!s) return;
    if (s->sh.vs) {
        ID3D11VertexShader_Release(s->sh.vs);
    }

    if (s->sh.ps) {
        ID3D11PixelShader_Release(s->sh.ps);
    }

    if (s->sh.vs_blob) {
        ID3D10Blob_Release(s->sh.vs_blob);
    }
    free(s);
}

static rhi_pipeline_t *dx11_create_pipeline(rhi_device_t *d, const rhi_pipeline_desc_t *pd) {
    if (!d || !pd || !pd->shader) return NULL;
    rhi_pipeline_t *p = (rhi_pipeline_t*)calloc(1, sizeof(rhi_pipeline_t));
    p->p.sh = pd->shader;

    D3D11_INPUT_ELEMENT_DESC il[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)(sizeof(float) * 3), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    HRESULT hr = ID3D11Device_CreateInputLayout(d->st->dev, il, 2,
                                                ID3D10Blob_GetBufferPointer(p->p.sh->sh.vs_blob),
                                                (SIZE_T)ID3D10Blob_GetBufferSize(p->p.sh->sh.vs_blob),
                                                &p->p.il);
    if (FAILED(hr)) {
        free(p);
        return NULL;
    }
    return p;
}

static void dx11_destroy_pipeline(rhi_device_t *d, rhi_pipeline_t *p) {
    UNUSED(d);

    if (!p) return;
    if (p->p.il) {
        ID3D11InputLayout_Release(p->p.il);
    }

    free(p);
}

static rhi_cmd_t *dx11_begin_cmd(rhi_device_t *d) {
    rhi_cmd_t *c = (rhi_cmd_t*)calloc(1, sizeof(rhi_cmd_t));
    if (!c) return NULL;
    c->st = d->st;
    return c;
}

static void dx11_end_cmd(rhi_cmd_t *c) {
    free(c);
}

static void dx11_cmd_begin_render(rhi_cmd_t *c, rhi_texture_t *color, rhi_texture_t *depth, const float clear[4]) {
    UNUSED(color);
    UNUSED(depth);

    ID3D11DeviceContext_OMSetRenderTargets(c->st->ctx, 1, &c->st->rtv, NULL);
    if (clear) {
        ID3D11DeviceContext_ClearRenderTargetView(c->st->ctx, c->st->rtv, clear);
    }

    D3D11_VIEWPORT vp = {0};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = (FLOAT)c->st->w;
    vp.Height = (FLOAT)c->st->h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ID3D11DeviceContext_RSSetViewports(c->st->ctx, 1, &vp);
}

static void dx11_cmd_end_render(rhi_cmd_t *c) {
    UNUSED(c);
}

static void dx11_cmd_bind_pipeline(rhi_cmd_t *c, rhi_pipeline_t *p) {
    ID3D11DeviceContext_IASetInputLayout(c->st->ctx, p->p.il);
    ID3D11DeviceContext_VSSetShader(c->st->ctx, p->p.sh->sh.vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(c->st->ctx, p->p.sh->sh.ps, NULL, 0);
    ID3D11DeviceContext_IASetPrimitiveTopology(c->st->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

static void dx11_cmd_bind_set(rhi_cmd_t *c, const rhi_binding_t *binds, int num, uint32_t stages) {
    UNUSED(c);
    UNUSED(binds);
    UNUSED(num);
    UNUSED(stages);
}

static void dx11_cmd_set_viewport_scissor(rhi_cmd_t *c, int x, int y, int w, int h) {
    UNUSED(c);

    dx11_state_t *st = c->st;
    D3D11_VIEWPORT vp;
    vp.TopLeftX = (FLOAT)x;
    vp.TopLeftY = (FLOAT)y;
    vp.Width = (FLOAT)w;
    vp.Height = (FLOAT)h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ID3D11DeviceContext_RSSetViewports(st->ctx, 1, &vp);
}

static void dx11_cmd_set_vertex_buffer(rhi_cmd_t *c, int slot, rhi_buffer_t *b) {
    b->vb.stride = sizeof(float) * 6;
    b->vb.offset = 0;
    ID3D11Buffer *buf = b->vb.buf;
    ID3D11DeviceContext_IASetVertexBuffers(c->st->ctx, (UINT)slot, 1, &buf, &b->vb.stride, &b->vb.offset);
}

static void dx11_cmd_set_index_buffer(rhi_cmd_t *c, rhi_buffer_t *b) {
    UNUSED(c);
    UNUSED(b);
}

static void dx11_cmd_draw(rhi_cmd_t *c, uint32_t vtx_count, uint32_t first, uint32_t inst) {
    UNUSED(inst);
    ID3D11DeviceContext_Draw(c->st->ctx, (UINT)vtx_count, (UINT)first);
}

static void dx11_cmd_draw_indexed(rhi_cmd_t *c, uint32_t idx_count, uint32_t first, uint32_t base_vtx, uint32_t inst) {
    UNUSED(c);
    UNUSED(idx_count);
    UNUSED(first);
    UNUSED(base_vtx);
    UNUSED(inst);
}

static rhi_fence_t *dx11_fence_create(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_fence_t*)calloc(1, sizeof(rhi_fence_t));
}

static void dx11_fence_wait(rhi_fence_t *f) {
    UNUSED(f);
}

static void dx11_fence_destroy(rhi_fence_t *f) {
    free(f);
}

static void dx11_get_capabilities(rhi_device_t *dev, rhi_capabilities_t *out) {
    (void)dev;
    out->min_depth = 0.0f;
    out->max_depth = 1.0f;
    out->conventions.uv_yaxis = RHI_AXIS_DOWN;
    out->conventions.ndc_yaxis = RHI_AXIS_UP;
    out->conventions.matrix_order = RHI_MATRIX_ROW_MAJOR;
}

PLUGIN_API const rhi_dispatch_t *maru_rhi_entry(void) {
    static const rhi_dispatch_t vtbl = {
        /* device */
        dx11_create_device, dx11_destroy_device,
        /* swapchain */
        dx11_get_swapchain, dx11_present,
        /* resources */
        dx11_create_buffer, dx11_destroy_buffer,
        dx11_create_texture, dx11_destroy_texture,
        dx11_create_shader, dx11_destroy_shader,
        dx11_create_pipeline, dx11_destroy_pipeline,
        /* commands */
        dx11_begin_cmd, dx11_end_cmd, dx11_cmd_begin_render, dx11_cmd_end_render,
        dx11_cmd_bind_pipeline, dx11_cmd_bind_set,
        dx11_cmd_set_viewport_scissor, dx11_cmd_set_vertex_buffer, dx11_cmd_set_index_buffer,
        dx11_cmd_draw, dx11_cmd_draw_indexed,
        /* sync */
        dx11_fence_create, dx11_fence_wait, dx11_fence_destroy,
        dx11_get_capabilities,
    };
    return &vtbl;
}
