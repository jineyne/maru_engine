#include "engine.h"

#include "config.h"
#include "plugin/plugin.h"
#include "rhi/rhi.h"
#include "engine_context.h"

#include <string.h>

#include "platform/window.h"


static engine_context_t g_ctx;

static int initialized = 0;

static const char *map_backend_to_regname(const char *backend) {
    if (!backend) return "gl";
    if (strcmp(backend, "dx") == 0 || strcmp(backend, "dx11") == 0) return "dx11";
    if (strcmp(backend, "gles") == 0) return "gles";
    return "gl";
}

static const rhi_backend map_backend_to_backend_key(const char* backend) {
    if (!backend) RHI_BACKEND_GL;
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

    // only for android
    // engine_context_load_rhi(&g_ctx, "maru-gles", "gles");
#if defined(_WIN32)
    engine_context_load_rhi(&g_ctx, "maru-dx11", "dx11");
#endif

    const char *want = map_backend_to_regname(cfg.graphics_backend);

    int win_w = 1024, win_h = 768, win_vsync = 1;
    g_ctx.window = platform_window_create(/*native_handle=*/NULL, win_w, win_h, win_vsync);
    if (!g_ctx.window) {
        ERROR("Failed to create platform window");
        engine_context_shutdown(&g_ctx);
        return MARU_ERR_INVALID;
    }

    rhi_device_desc_t device_desc = {0};
    device_desc.backend = map_backend_to_backend_key(want);
    device_desc.native_window = NULL;
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

    initialized = 1;
    return MARU_OK;
}

bool maru_engine_tick() {
    if (!initialized) return false;
    if (platform_should_close(g_ctx.window)) return false;

    platform_poll_events();
}

void maru_engine_shutdown(void) {
    if (!initialized) return;

    engine_context_shutdown(&g_ctx);

    INFO("maru shutdown");

    initialized = 0;
}
