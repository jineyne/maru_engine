#ifndef MARU_PLUGIN_LOADER_H
#define MARU_PLUGIN_LOADER_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct plugin_handler {
    void *lib;
} plugin_handler_t;

typedef int (*maru_plugin_init_fn)(void);
typedef void (*maru_plugin_shutdown_fn)(void);

plugin_handler_t load_plugin(const char *basename);
void *plugin_get_symbol(const plugin_handler_t *h, const char *symbol_name);
void unload_plugin(plugin_handler_t *h);
static inline int plugin_is_valid(const plugin_handler_t *h) { return h && h->lib != NULL; }

#ifdef __cplusplus
}
#endif

#endif /* MARU_PLUGIN_LOADER_H */
