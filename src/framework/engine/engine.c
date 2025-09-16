#include "engine.h"

#include "config.h"
#include "plugin/plugin.h"
#include "rhi/rhi.h"

#include <string.h>

static int initialized = 0;

plugin_handler_t g_rhi_plugin;

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

    const char *rhi_base = (cfg.graphics_backend && strcmp(cfg.graphics_backend, "dx11") == 0)
                               ? "maru-dx11"
                               : "maru-gl";

    g_rhi_plugin = load_plugin(rhi_base);
    if (!plugin_is_valid(&g_rhi_plugin)) {
        ERROR("rhi plugin load failed: %s", rhi_base);
        return MARU_ERR_INVALID;
    }

    maru_plugin_init_fn initf = (maru_plugin_init_fn)plugin_get_symbol(&g_rhi_plugin, "maru_plugin_init");
    if (initf) {
        if (initf() != 0) {
            ERROR("plugin init failed: %s", rhi_base);
            unload_plugin(&g_rhi_plugin);
            return MARU_ERR_INVALID;
        }
    }

    maru_rhi_entry_fn rhi_entry = (maru_rhi_entry_fn)plugin_get_symbol(&g_rhi_plugin, "maru_rhi_entry");
    if (!rhi_entry) {
        ERROR("rhi entry not found in plugin: %s", rhi_base);

        maru_plugin_shutdown_fn sh = (maru_plugin_shutdown_fn)plugin_get_symbol(&g_rhi_plugin, "maru_plugin_shutdown");
        if (sh) sh();
        unload_plugin(&g_rhi_plugin);
        return MARU_ERR_INVALID;
    }

    const rhi_dispatch_t *disp = rhi_entry();
    if (!disp) {
        ERROR("rhi dispatch is NULL: %s", rhi_base);
        maru_plugin_shutdown_fn sh = (maru_plugin_shutdown_fn)plugin_get_symbol(&g_rhi_plugin, "maru_plugin_shutdown");
        if (sh) sh();
        unload_plugin(&g_rhi_plugin);
        return MARU_ERR_INVALID;
    }

    rhi_device_desc_t desc = {0};
    desc.backend = (strcmp(rhi_base, "maru-dx11") == 0) ? RHI_BACKEND_DX11 : RHI_BACKEND_GL;
    desc.width = 1024;
    desc.height = 768;
    desc.vsync = true;

    rhi_device_t *device = disp->create_device(&desc);
    if (!device) {
        ERROR("failed to create rhi device");
        maru_plugin_shutdown_fn sh = (maru_plugin_shutdown_fn)plugin_get_symbol(&g_rhi_plugin, "maru_plugin_shutdown");
        if (sh) sh();
        unload_plugin(&g_rhi_plugin);
        return MARU_ERR_INVALID;
    }
    disp->destroy_device(device);

    initialized = 1;
    return MARU_OK;
}

void maru_engine_shutdown(void) {
    if (!initialized) return;

    INFO("maru shutdown");

    maru_plugin_shutdown_fn sh = (maru_plugin_shutdown_fn)plugin_get_symbol(&g_rhi_plugin, "maru_plugin_shutdown");
    if (sh) sh();

    unload_plugin(&g_rhi_plugin);

    initialized = 0;
}
