#include "plugin.h"

#include <string.h>

#include "core.h"

#if defined(MARU_PLATFORM_WINDOWS)
#include <windows.h>
#define DYNLIB HMODULE
#define LOAD_LIB(path) LoadLibraryA(path)
#define LOAD_SYM(lib, name) GetProcAddress(lib, name)
#define FREE_LIB(lib) FreeLibrary(lib)
#define MARU_DLL_EXT ".dll"
#elif defined(MARU_PLATFORM_ANDROID) || defined(__linux__)
#include <dlfcn.h>
#define DYNLIB void*
#define LOAD_LIB(path) dlopen(path, RTLD_NOW)
#define LOAD_SYM(lib, name) dlsym(lib, name)
#define FREE_LIB(lib) dlclose(lib)
#define MARU_DLL_EXT ".so"
#else
#define DYNLIB void*
#define LOAD_LIB(path) NULL
#define LOAD_SYM(lib, name) NULL
#define FREE_LIB(lib) (void)0
#define MARU_DLL_EXT ".dylib"
#endif

static DYNLIB try_load_variants_internal(const char *base) {
    char buf[512];

    snprintf(buf, sizeof(buf), "%s" MARU_DLL_EXT, base);
    DYNLIB lib = LOAD_LIB(buf);
    if (lib) {
        INFO("Loaded plugin: %s", buf);
        return lib;
    }

#if defined(__linux__) || defined(ANDROID) || defined(__APPLE__)
    snprintf(buf, sizeof(buf), "lib%s" MARU_DLL_EXT, base);
    lib = LOAD_LIB(buf);
    if (lib) {
        INFO("Loaded plugin: %s", buf);
        return lib;
    }
#endif

    lib = LOAD_LIB(base);
    if (lib) {
        INFO("loaded plugin (raw): %s", base);
        return lib;
    }

    return NULL;
}

plugin_handler_t load_plugin(const char *basename) {
    plugin_handler_t h = {.lib = NULL};
    if (!basename) {
        maru_log(MARU_LOG_ERROR, "load_plugin: basename is NULL");
        return h;
    }

#ifdef MARU_PLATFORM_IOS
    INFO("load_plugin: dynamic loading skipped on iOS");
    UNUSED(basename);
    return h;
#else
    DYNLIB lib = try_load_variants_internal(basename);
    if (!lib) {
        maru_log(MARU_LOG_ERROR, "load_plugin: failed to load plugin '%s'", basename);
        return h;
    }
    h.lib = lib;
    return h;
#endif
}

void *plugin_get_symbol(const plugin_handler_t *h, const char *symbol_name) {
    if (!h || !h->lib || !symbol_name) return NULL;
    void *sym = (void*)LOAD_SYM(h->lib, symbol_name);
    if (!sym) {
        DEBUG_LOG("plugin_get_symbol: symbol '%s' not found", symbol_name);
    }
    return sym;
}

void unload_plugin(plugin_handler_t *h) {
    if (!h) return;
    if (!h->lib) {
        return;
    }

    FREE_LIB(h->lib);
    h->lib = NULL;

    INFO("plugin unloaded");
}
