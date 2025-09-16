#include "engine_context.h"

#include <string.h>

#include "platform/window.h"

typedef int (*maru_plugin_init_fn)(void);
typedef void (*maru_plugin_shutdown_fn)(void);
typedef const rhi_dispatch_t * (*maru_rhi_entry_fn)(void);

static void shutdown_slot(plugin_handler_t *ph) {
    if (!plugin_is_valid(ph)) return;
    maru_plugin_shutdown_fn sh = (maru_plugin_shutdown_fn)plugin_get_symbol(ph, "maru_plugin_shutdown");
    if (sh) sh();
    unload_plugin(ph);
}

void engine_context_init(engine_context_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->lock = maru_mutex_create();
    ctx->window = NULL;
}

void engine_context_shutdown(engine_context_t *ctx) {
    if (!ctx) return;

    maru_mutex_lock(ctx->lock);

    if (ctx->active_rhi && ctx->active_device) {
        ctx->active_rhi->destroy_device(ctx->active_device);
        ctx->active_device = NULL;
        ctx->active_rhi = NULL;
    }

    for (int i = 0; i < ctx->rhi_count; ++i) {
        shutdown_slot(&ctx->rhi[i].ph);
        memset(&ctx->rhi[i], 0, sizeof(ctx->rhi[i]));
    }
    ctx->rhi_count = 0;

    for (int s = 0; s < MARU_PLUGIN_SLOT_MAX; ++s) {
        shutdown_slot(&ctx->slots[s]);
        memset(&ctx->slots[s], 0, sizeof(ctx->slots[s]));
    }

    if (ctx->window) {
        platform_window_destroy(ctx->window);
        ctx->window = NULL;
    }

    maru_mutex_unlock(ctx->lock);
    maru_mutex_destroy(ctx->lock);
    ctx->lock = NULL;
}

int engine_context_load_plugin(engine_context_t *ctx, maru_plugin_slot_e slot, const char *basename) {
    if (!ctx || !basename) {
        return MARU_ERR_INVALID;
    }

    maru_mutex_lock(ctx->lock);

    if (slot < 0 || slot >= MARU_PLUGIN_SLOT_MAX) {
        maru_mutex_unlock(ctx->lock);
        return MARU_ERR_INVALID;
    }
    if (plugin_is_valid(&ctx->slots[slot])) {
        maru_mutex_unlock(ctx->lock);
        return MARU_ERR_INVALID;
    }

    maru_mutex_unlock(ctx->lock);

    plugin_handler_t ph = load_plugin(basename);
    if (!plugin_is_valid(&ph)) {
        return MARU_ERR_IO;
    }

    maru_plugin_init_fn initf = (maru_plugin_init_fn)plugin_get_symbol(&ph, "maru_plugin_init");
    if (initf && initf() != 0) {
        unload_plugin(&ph);
        return MARU_ERR_INVALID;
    }

    maru_mutex_lock(ctx->lock);
    if (plugin_is_valid(&ctx->slots[slot])) {
        maru_mutex_unlock(ctx->lock);
        shutdown_slot(&ph);
        return MARU_ERR_INVALID;
    }

    ctx->slots[slot] = ph;
    maru_mutex_unlock(ctx->lock);

    return MARU_OK;
}

int engine_context_load_rhi(engine_context_t *ctx, const char *soname, const char *reg_name) {
    if (!ctx || !soname || !reg_name) return MARU_ERR_INVALID;

    plugin_handler_t ph = load_plugin(soname);
    if (!plugin_is_valid(&ph)) {
        return MARU_ERR_IO;
    }

    maru_plugin_init_fn initf = (maru_plugin_init_fn)plugin_get_symbol(&ph, "maru_plugin_init");
    if (initf && initf() != 0) {
        unload_plugin(&ph);
        return MARU_ERR_INVALID;
    }

    maru_rhi_entry_fn entry = (maru_rhi_entry_fn)plugin_get_symbol(&ph, "maru_rhi_entry");
    if (!entry) {
        shutdown_slot(&ph);
        return MARU_ERR_INVALID;
    }
    const rhi_dispatch_t *disp = entry();
    if (!disp) {
        shutdown_slot(&ph);
        return MARU_ERR_INVALID;
    }

    maru_mutex_lock(ctx->lock);

    if (ctx->rhi_count >= (int)(sizeof(ctx->rhi) / sizeof(ctx->rhi[0]))) {
        maru_mutex_unlock(ctx->lock);
        shutdown_slot(&ph);
        return MARU_ERR_INVALID;
    }

    for (int i = 0; i < ctx->rhi_count; ++i) {
        if (strcmp(ctx->rhi[i].name, reg_name) == 0) {
            maru_mutex_unlock(ctx->lock);
            shutdown_slot(&ph);
            return MARU_ERR_INVALID;
        }
    }

    rhi_backend_entry_t *e = &ctx->rhi[ctx->rhi_count++];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, reg_name, sizeof(e->name) - 1);
    e->disp = disp;
    e->ph = ph;

    maru_mutex_unlock(ctx->lock);
    return MARU_OK;
}

int engine_context_select_rhi(engine_context_t *ctx, const char *reg_name, const rhi_device_desc_t *desc_in) {
    if (!ctx || !reg_name) return MARU_ERR_INVALID;

    maru_mutex_lock(ctx->lock);

    const rhi_dispatch_t *disp = NULL;
    for (int i = 0; i < ctx->rhi_count; ++i) {
        if (strcmp(ctx->rhi[i].name, reg_name) == 0) {
            disp = ctx->rhi[i].disp;
            break;
        }
    }
    if (!disp) {
        maru_mutex_unlock(ctx->lock);
        return MARU_ERR_INVALID;
    }

    if (ctx->active_rhi && ctx->active_device) {
        ctx->active_rhi->destroy_device(ctx->active_device);
        ctx->active_device = NULL;
        ctx->active_rhi = NULL;
    }

    rhi_device_desc_t desc = {0};
    if (desc_in) desc = *desc_in;

    maru_mutex_unlock(ctx->lock);

    rhi_device_t *dev = disp->create_device(&desc);
    if (!dev) return MARU_ERR_IO;

    maru_mutex_lock(ctx->lock);
    ctx->active_rhi = disp;
    ctx->active_device = dev;
    maru_mutex_unlock(ctx->lock);

    return MARU_OK;
}
