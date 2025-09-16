#ifndef MARU_ENGINE_CONTEXT_H
#define MARU_ENGINE_CONTEXT_H

#include "core.h"

#include "plugin/plugin.h"
#include "rhi/rhi.h"
#include "thread/mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MARU_PLUGIN_SLOT_RHI = 0,
    MARU_PLUGIN_SLOT_AUDIO = 1,
    MARU_PLUGIN_SLOT_MAX
} maru_plugin_slot_e;

typedef struct rhi_backend_entry {
    char name[16];
    const rhi_dispatch_t *disp;
    plugin_handler_t ph;
} rhi_backend_entry_t;

typedef struct engine_context {
    mutex_t *lock;

    plugin_handler_t slots[MARU_PLUGIN_SLOT_MAX];

    rhi_backend_entry_t rhi[4];
    int rhi_count;

    const rhi_dispatch_t *active_rhi;
    rhi_device_t *active_device;

    struct platform_window *window;
} engine_context_t;

void engine_context_init(engine_context_t *ctx);
void engine_context_shutdown(engine_context_t *ctx);

int engine_context_load_plugin(engine_context_t *ctx, maru_plugin_slot_e slot, const char *basename);
int engine_context_load_rhi(engine_context_t *ctx, const char *soname, const char *reg_name);
int engine_context_select_rhi(engine_context_t *ctx, const char *reg_name, const rhi_device_desc_t *desc_in);

static inline void *engine_context_get_symbol(const plugin_handler_t *ph, const char *sym) {
    return plugin_get_symbol(ph, sym);
}

#ifdef __cplusplus
}
#endif
#endif /* MARU_ENGINE_CONTEXT_H */
