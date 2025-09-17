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
    rhi_render_target_t *back_rt;
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

typedef struct {
    ID3D11Texture2D *tex;
    ID3D11ShaderResourceView *srv;
    ID3D11RenderTargetView *rtv;
    ID3D11DepthStencilView *dsv;
    DXGI_FORMAT fmt;
    int w, h, mips;
} dx11_tex_t;

struct rhi_texture {
    dx11_tex_t t;
};

typedef struct {
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3DBlob *vs_blob;
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

typedef struct dx11_rt {
    dx11_state_t *st;
    ID3D11RenderTargetView *rtvs[4];
    int color_count;
    ID3D11DepthStencilView *dsv;
    int is_backbuffer;
} dx11_rt_t;

struct rhi_render_target {
    dx11_rt_t rt;
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

static void dx11_refresh_backbuffer_rt(dx11_state_t *st) {
    if (!st || !st->rtv) return;
    if (!st->back_rt) {
        st->back_rt = (rhi_render_target_t*) calloc(1, sizeof(rhi_render_target_t));
    }

    dx11_rt_t *rt = &st->back_rt->rt;
    memset(rt, 0, sizeof(*rt));
    rt->st = st;
    rt->is_backbuffer = 1;
    rt->color_count = 1;
    rt->rtvs[0] = st->rtv;
    rt->dsv = NULL;
}

static void dx11_drop_backbuffer_rt(dx11_state_t *st) {
    if (!st) return;
    if (st->back_rt) {
        free(st->back_rt);
        st->back_rt = NULL;
    }
}

static DXGI_FORMAT map_depth_format(rhi_format f) {
    return DXGI_FORMAT_D24_UNORM_S8_UINT;
}

static DXGI_FORMAT map_color_format(rhi_format f) {
    switch (f) {
    case RHI_FMT_RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case RHI_FMT_BGRA8: return DXGI_FORMAT_B8G8R8A8_UNORM;
    default: return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

static void dx11_unbind_all_srvs(dx11_state_t *st) {
    if (!st || !st->ctx) return;

    ID3D11ShaderResourceView *nulls[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
    ID3D11DeviceContext_PSSetShaderResources(st->ctx, 0, 128, nulls);
    ID3D11DeviceContext_VSSetShaderResources(st->ctx, 0, 128, nulls);
}

static int dx11_srv_conflicts_with_rt(ID3D11ShaderResourceView *srv, dx11_rt_t *rt) {
    if (!srv || !rt) return 0;

    ID3D11Resource *srv_res = NULL;
    ID3D11ShaderResourceView_GetResource(srv, &srv_res);
    if (!srv_res) return 0;

    int conflict = 0;

    for (int i = 0; i < rt->color_count; ++i) {
        if (!rt->rtvs[i]) continue;
        ID3D11Resource *rt_res = NULL;
        ID3D11RenderTargetView_GetResource(rt->rtvs[i], &rt_res);
        if (rt_res) {
            if (rt_res == srv_res) conflict = 1;
            ID3D11Resource_Release(rt_res);
        }
        if (conflict) break;
    }

    if (!conflict && rt->dsv) {
        ID3D11Resource *ds_res = NULL;
        ID3D11DepthStencilView_GetResource(rt->dsv, &ds_res);
        if (ds_res) {
            if (ds_res == srv_res) conflict = 1;
            ID3D11Resource_Release(ds_res);
        }
    }

    ID3D11Resource_Release(srv_res);
    return conflict;
}

#pragma endregion helpers

static rhi_device_t *dx11_create_device(const rhi_device_desc_t *desc) {
    if (!desc || !desc->native_window) return NULL;

    dx11_state_t *st = (dx11_state_t*) calloc(1, sizeof(dx11_state_t));
    if (!st) return NULL;

    st->hwnd = (HWND) desc->native_window;
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
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    const D3D_FEATURE_LEVEL fls[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL got = 0;

    UINT flags = 0;
#if !defined(NDEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, fls, ARRAY_SIZE(fls),
                                               D3D11_SDK_VERSION, &sd, &st->sc, &st->dev, &got, &st->ctx);
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

    dx11_refresh_backbuffer_rt(st);

    rhi_device_t *dev = (rhi_device_t*) calloc(1, sizeof(rhi_device_t));
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
    rhi_swapchain_t *s = (rhi_swapchain_t*) calloc(1, sizeof(rhi_swapchain_t));
    if (!s) return NULL;
    s->st = desc->st;
    return s;
}

static void dx11_present(rhi_swapchain_t *s) {
    if (!s || !s->st || !s->st->sc) return;
    IDXGISwapChain_Present(s->st->sc, s->st->vsync ? 1 : 0, 0);
}

static void dx11_resize(rhi_device_t *d, int new_w, int new_h) {
    if (!d || !d->st || !d->st->sc) return;
    dx11_state_t *st = d->st;

    if (new_w <= 0 || new_h <= 0) return;
    if (st->w == new_w && st->h == new_h) return;

    ID3D11DeviceContext_OMSetRenderTargets(st->ctx, 0, NULL, NULL);
    dx11_unbind_all_srvs(st);
    ID3D11DeviceContext_ClearState(st->ctx);
    ID3D11DeviceContext_Flush(st->ctx);

    if (st->rtv) {
        ID3D11RenderTargetView_Release(st->rtv);
        st->rtv = NULL;
    }
    dx11_drop_backbuffer_rt(st);

    HRESULT hr = IDXGISwapChain_ResizeBuffers(st->sc, 0, (UINT)new_w, (UINT)new_h, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        FATAL("DX11 ResizeBuffers failed: 0x%08X", (unsigned)hr);
        return;
    }

    if (dx11_create_backbuffer_rtv(st) != 0) {
        FATAL("DX11 recreate RTV failed after resize");
        return;
    }

    dx11_refresh_backbuffer_rt(st);

    st->w = new_w;
    st->h = new_h;
}

static rhi_buffer_t *dx11_create_buffer(rhi_device_t *d, const rhi_buffer_desc_t *desc, const void *initial) {
    if (!d || !d->st || !desc || desc->size == 0) return NULL;

    D3D11_BUFFER_DESC bd = {0};
    bd.ByteWidth = (UINT) ALIGN_UP(desc->size, 16);
    bd.Usage = D3D11_USAGE_DEFAULT;

    if (desc->usage & RHI_BUF_VERTEX) {
        bd.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
    }

    if (desc->usage & RHI_BUF_INDEX) {
        bd.BindFlags |= D3D11_BIND_INDEX_BUFFER;
    }

    if (desc->usage & RHI_BUF_CONST) {
        bd.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
    }

    if (bd.Usage == D3D11_USAGE_DYNAMIC) {
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    }

    D3D11_SUBRESOURCE_DATA srd = {0};
    srd.pSysMem = initial;

    ID3D11Buffer *buf = NULL;
    HRESULT hr = ID3D11Device_CreateBuffer(d->st->dev, &bd, initial ? &srd : NULL, &buf);
    if (FAILED(hr)) return NULL;

    rhi_buffer_t *b = (rhi_buffer_t*) calloc(1, sizeof(rhi_buffer_t));
    b->vb.buf = buf;
    b->vb.size = desc->size;
    b->vb.stride = 0;
    b->vb.offset = 0;
    return b;
}

static void dx11_update_buffer(rhi_device_t *d, rhi_buffer_t *b, const void *data, size_t bytes) {
    if (!d || !d->st || !b || !b->vb.buf || !data || bytes == 0) return;

    D3D11_BUFFER_DESC bd;
    ID3D11Buffer_GetDesc(b->vb.buf, &bd);
    if (bd.Usage == D3D11_USAGE_DYNAMIC) {
        D3D11_MAPPED_SUBRESOURCE ms;
        if (SUCCEEDED(ID3D11DeviceContext_Map(d->st->ctx, (ID3D11Resource *)b->vb.buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            memcpy(ms.pData, data, bytes);
            ID3D11DeviceContext_Unmap(d->st->ctx, (ID3D11Resource *)b->vb.buf, 0);
        }
    } else {
        ID3D11DeviceContext_UpdateSubresource(d->st->ctx, (ID3D11Resource *)b->vb.buf, 0, NULL, data, 0, 0);
    }
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
    UNUSED(initial);

    dx11_state_t *st = ((struct rhi_device*) d)->st;
    HRESULT hr = S_OK;

    const bool wantSRV = (desc->usage & RHI_TEX_USAGE_SAMPLED) != 0;
    const bool wantRTV = (desc->usage & RHI_TEX_USAGE_RENDER_TARGET) != 0;
    const bool wantDSV = (desc->usage & RHI_TEX_USAGE_DEPTH) != 0;

    DXGI_FORMAT base_fmt = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT srv_fmt = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT rtv_fmt = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dsv_fmt = DXGI_FORMAT_UNKNOWN;

    if (wantDSV) {
        if (wantSRV) {
            base_fmt = DXGI_FORMAT_R24G8_TYPELESS;
            dsv_fmt = DXGI_FORMAT_D24_UNORM_S8_UINT;
            srv_fmt = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        } else {
            base_fmt = DXGI_FORMAT_D24_UNORM_S8_UINT;
            dsv_fmt = DXGI_FORMAT_D24_UNORM_S8_UINT;
        }
    } else {
        base_fmt = (desc->format == RHI_FMT_BGRA8) ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
        if (wantSRV) srv_fmt = base_fmt;
        if (wantRTV) rtv_fmt = base_fmt;
    }


    rhi_texture_t *tex = (rhi_texture_t*) calloc(1, sizeof(*tex));
    if (!tex) return NULL;

    D3D11_TEXTURE2D_DESC td = {0};
    td.Width = desc->width;
    td.Height = desc->height;
    td.MipLevels = desc->mip_levels > 0 ? desc->mip_levels : 1;
    td.ArraySize = 1;
    td.Format = base_fmt;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    if (wantRTV) td.BindFlags |= D3D11_BIND_RENDER_TARGET;
    if (wantSRV) td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    if (wantDSV) td.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

    const D3D11_SUBRESOURCE_DATA *init_data = NULL;
    hr = ID3D11Device_CreateTexture2D(st->dev, &td, NULL, &tex->t.tex);
    if (FAILED(hr)) {
        free(tex);
        return NULL;
    }

    if (wantSRV) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd = {0};
        sd.Format = srv_fmt;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = td.MipLevels;

        hr = ID3D11Device_CreateShaderResourceView(st->dev, (ID3D11Resource*)tex->t.tex, &sd, &tex->t.srv);
        if (FAILED(hr)) {
            FATAL("CreateSRV failed 0x%08X", hr);
            goto fail;
        }
    }

    if (wantRTV) {
        D3D11_RENDER_TARGET_VIEW_DESC rvd = {0};
        rvd.Format = rtv_fmt;
        rvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rvd.Texture2D.MipSlice = 0;

        hr = ID3D11Device_CreateRenderTargetView(st->dev, (ID3D11Resource*)tex->t.tex, &rvd, &tex->t.rtv);
        if (FAILED(hr)) {
            MR_LOG(ERROR, "CreateRTV failed 0x%08X", hr);
            goto fail;
        }
    }

    if (wantDSV) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dvd = {0};
        dvd.Format = dsv_fmt;
        dvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dvd.Texture2D.MipSlice = 0;

        hr = ID3D11Device_CreateDepthStencilView(st->dev, (ID3D11Resource*)tex->t.tex, &dvd, &tex->t.dsv);
        if (FAILED(hr)) {
            MR_LOG(ERROR, "CreateDSV failed 0x%08X", hr);
            goto fail;
        }
    }


    tex->t.w = desc->width;
    tex->t.h = desc->height;
    tex->t.mips = td.MipLevels;
    tex->t.fmt = td.Format;
    return tex;

fail:
    if (tex->t.srv) {
        ID3D11ShaderResourceView_Release(tex->t.srv);
    }

    if (tex->t.rtv) {
        ID3D11RenderTargetView_Release(tex->t.rtv);
    }

    if (tex->t.dsv) {
        ID3D11DepthStencilView_Release(tex->t.dsv);
    }

    if (tex->t.tex) {
        ID3D11Texture2D_Release(tex->t.tex);
    }

    free(tex);
    return NULL;
}

static void dx11_destroy_texture(rhi_device_t *d, rhi_texture_t *t) {
    UNUSED(d);

    if (!t) return;

    if (t->t.srv) {
        ID3D11ShaderResourceView_Release(t->t.srv);
    }

    if (t->t.rtv) {
        ID3D11RenderTargetView_Release(t->t.rtv);
    }

    if (t->t.dsv) {
        ID3D11DepthStencilView_Release(t->t.dsv);
    }

    if (t->t.tex) {
        ID3D11Texture2D_Release(t->t.tex);
    }

    free(t);
}

static rhi_shader_t *dx11_create_shader(rhi_device_t *d, const rhi_shader_desc_t *sd) {
    if (!d || !sd || !sd->entry_vs || !sd->entry_ps) return NULL;

    ID3DBlob *vsb = NULL, *psb = NULL, *err = NULL;
    HRESULT hr = D3DCompile(sd->blob_vs, sd->blob_vs_size, "vs", NULL, NULL, sd->entry_vs, "vs_5_0", 0, 0, &vsb, &err);
    if (FAILED(hr)) {
        if (err) {
            const char *msg = (const char*) ID3D10Blob_GetBufferPointer(err);
            size_t mlen = (size_t) ID3D10Blob_GetBufferSize(err);
            INFO("HLSL VS compile error:\n%.*s", (int)mlen, msg);
            ID3D10Blob_Release(err);
        }

        return NULL;
    }
    hr = D3DCompile(sd->blob_ps, sd->blob_ps_size, "ps", NULL, NULL, sd->entry_ps, "ps_5_0", 0, 0, &psb, &err);
    if (FAILED(hr)) {
        ID3D10Blob_Release(vsb);
        if (err) {
            const char *msg = (const char*) ID3D10Blob_GetBufferPointer(err);
            size_t mlen = (size_t) ID3D10Blob_GetBufferSize(err);
            INFO("HLSL PS compile error:\n%.*s", (int)mlen, msg);
            ID3D10Blob_Release(err);
        }
        return NULL;
    }

    ID3D11VertexShader *vs = NULL;
    ID3D11PixelShader *ps = NULL;
    hr = ID3D11Device_CreateVertexShader(d->st->dev, ID3D10Blob_GetBufferPointer(vsb), (SIZE_T)ID3D10Blob_GetBufferSize(vsb), NULL, &vs);
    if (FAILED(hr)) {
        ID3D10Blob_Release(vsb);
        ID3D10Blob_Release(psb);
        return NULL;
    }
    hr = ID3D11Device_CreatePixelShader(d->st->dev, ID3D10Blob_GetBufferPointer(psb), (SIZE_T)ID3D10Blob_GetBufferSize(psb), NULL, &ps);
    if (FAILED(hr)) {
        ID3D11VertexShader_Release(vs);
        ID3D10Blob_Release(vsb);
        ID3D10Blob_Release(psb);
        return NULL;
    }

    rhi_shader_t *sh = (rhi_shader_t*) calloc(1, sizeof(rhi_shader_t));
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
    rhi_pipeline_t *p = (rhi_pipeline_t*) calloc(1, sizeof(rhi_pipeline_t));
    p->p.sh = pd->shader;

    D3D11_INPUT_ELEMENT_DESC il[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT) (sizeof(float) * 3), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    HRESULT hr = ID3D11Device_CreateInputLayout(d->st->dev, il, 2, ID3D10Blob_GetBufferPointer(p->p.sh->sh.vs_blob), (SIZE_T)ID3D10Blob_GetBufferSize(p->p.sh->sh.vs_blob), &p->p.il);
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

static rhi_render_target_t *dx11_create_render_target(rhi_device_t *d, const rhi_render_target_desc_t *desc) {
    UNUSED(d);
    UNUSED(desc);

    return NULL;
}

static void dx11_destroy_render_target(rhi_device_t *d, rhi_render_target_t *rt) {
    UNUSED(d);
    if (!rt) return;

    dx11_rt_t *dxrt = &rt->rt;

    if (!dxrt->is_backbuffer) {
        for (int i = 0; i < dxrt->color_count; ++i) {
            if (dxrt->rtvs[i]) {
                ID3D11RenderTargetView_Release(dxrt->rtvs[i]);
                dxrt->rtvs[i] = NULL;
            }
        }
        if (dxrt->dsv) {
            ID3D11DepthStencilView_Release(dxrt->dsv);
            dxrt->dsv = NULL;
        }
    }

    free(rt);
}

static rhi_render_target_t *dx11_get_backbuffer_rt(rhi_device_t *d) {
    dx11_state_t *st = d->st;
    dx11_refresh_backbuffer_rt(st);
    return st->back_rt;
}

static rhi_texture_t *dx11_render_target_get_color_tex(rhi_render_target_t *rt, int index) {
    UNUSED(rt);
    UNUSED(index);
    return NULL;
}

static rhi_cmd_t *dx11_begin_cmd(rhi_device_t *d) {
    rhi_cmd_t *c = (rhi_cmd_t*) calloc(1, sizeof(rhi_cmd_t));
    if (!c) return NULL;
    c->st = d->st;
    return c;
}

static void dx11_end_cmd(rhi_cmd_t *c) {
    free(c);
}

static void dx11_cmd_begin_render(rhi_cmd_t *c, rhi_render_target_t *rt, const float clear_rgba[4]) {
    if (!c || !c->st) return;
    dx11_state_t *st = c->st;

    rhi_render_target_t *use_rt = rt ? rt : st->back_rt;
    if (!use_rt || !use_rt->rt.color_count || !use_rt->rt.rtvs[0]) {
        if (!st->rtv) {
            if (dx11_create_backbuffer_rtv(st) != 0) return;
        }
        dx11_refresh_backbuffer_rt(st);
        use_rt = st->back_rt;
        if (!use_rt || !use_rt->rt.color_count || !use_rt->rt.rtvs[0]) return;
    }

    ID3D11RenderTargetView *rtv = use_rt->rt.rtvs[0];
    ID3D11DeviceContext_OMSetRenderTargets(st->ctx, 1, &rtv, NULL);

    D3D11_VIEWPORT vp = {0};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = (FLOAT) st->w;
    vp.Height = (FLOAT) st->h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ID3D11DeviceContext_RSSetViewports(st->ctx, 1, &vp);

    if (clear_rgba) {
        ID3D11DeviceContext_ClearRenderTargetView(st->ctx, rtv, clear_rgba);
    }

    dx11_unbind_all_srvs(st);
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
    dx11_state_t *st = c->st;
    dx11_rt_t *cur = (st->back_rt ? &st->back_rt->rt : NULL);

    for (int i = 0; i < num; ++i) {
        const rhi_binding_t *b = &binds[i];
        if (!b->texture) continue;
        ID3D11ShaderResourceView *srv = b->texture->t.srv;
        if (!srv) continue;

        if (cur && dx11_srv_conflicts_with_rt(srv, cur)) {
            DEBUG_LOG("DX11: SRV-RTV conflict on slot %u -> skip", b->binding);
            continue;
        }

        if (stages & RHI_STAGE_PS) {
            ID3D11DeviceContext_PSSetShaderResources(st->ctx, b->binding, 1, &srv);
        }
        if (stages & RHI_STAGE_VS) {
            ID3D11DeviceContext_VSSetShaderResources(st->ctx, b->binding, 1, &srv);
        }
    }
}

static void dx11_cmd_bind_const_buffer(rhi_cmd_t *c, int slot, rhi_buffer_t *b, uint32_t stages) {
    if (!c || !c->st || !b || !b->vb.buf) return;
    if (stages & RHI_STAGE_VS) {
        ID3D11DeviceContext_VSSetConstantBuffers(c->st->ctx, (UINT)slot, 1, &b->vb.buf);
    }
    if (stages & RHI_STAGE_PS) {
        ID3D11DeviceContext_PSSetConstantBuffers(c->st->ctx, (UINT)slot, 1, &b->vb.buf);
    }
}

static void dx11_cmd_set_viewport_scissor(rhi_cmd_t *c, int x, int y, int w, int h) {
    UNUSED(c);

    dx11_state_t *st = c->st;
    D3D11_VIEWPORT vp;
    vp.TopLeftX = (FLOAT) x;
    vp.TopLeftY = (FLOAT) y;
    vp.Width = (FLOAT) w;
    vp.Height = (FLOAT) h;
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
    return (rhi_fence_t*) calloc(1, sizeof(rhi_fence_t));
}

static void dx11_fence_wait(rhi_fence_t *f) {
    UNUSED(f);
}

static void dx11_fence_destroy(rhi_fence_t *f) {
    free(f);
}

static void dx11_get_capabilities(rhi_device_t *dev, rhi_capabilities_t *out) {
    (void) dev;
    out->min_depth = 0.0f;
    out->max_depth = 1.0f;
    out->conventions.uv_yaxis = RHI_AXIS_DOWN;
    out->conventions.ndc_yaxis = RHI_AXIS_UP;
    out->conventions.matrix_order = RHI_MATRIX_COLUMN_MAJOR;
}

PLUGIN_API const rhi_dispatch_t *maru_rhi_entry(void) {
    static const rhi_dispatch_t vtbl = {
        /* device */
        dx11_create_device, dx11_destroy_device,
        /* swapchain */
        dx11_get_swapchain, dx11_present,
        /* resize */
        dx11_resize,
        /* resources */
        dx11_create_buffer, dx11_destroy_buffer, dx11_update_buffer,
        dx11_create_texture, dx11_destroy_texture,
        dx11_create_shader, dx11_destroy_shader,
        dx11_create_pipeline, dx11_destroy_pipeline,

        dx11_create_render_target, dx11_destroy_render_target,
        dx11_get_backbuffer_rt, dx11_render_target_get_color_tex,

        /* commands */
        dx11_begin_cmd, dx11_end_cmd, dx11_cmd_begin_render, dx11_cmd_end_render,
        dx11_cmd_bind_pipeline, dx11_cmd_bind_set, dx11_cmd_bind_const_buffer,
        dx11_cmd_set_viewport_scissor, dx11_cmd_set_vertex_buffer, dx11_cmd_set_index_buffer,
        dx11_cmd_draw, dx11_cmd_draw_indexed,
        /* sync */
        dx11_fence_create, dx11_fence_wait, dx11_fence_destroy,
        dx11_get_capabilities,
    };
    return &vtbl;
}
