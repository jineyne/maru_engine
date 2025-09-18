#include "asset.h"
#include "fs.h"
#include "log.h"
#include "platform/path.h"

#include <string.h>
#include <stdlib.h>

static char g_asset_root[512] = {0};

static void path_dirname_inplace(char *p) {
    if (!p) return;
    size_t n = strlen(p);
    for (size_t i = n; i > 0; --i) {
        char c = p[i - 1];
        if (c == '/' || c == '\\') {
            p[i - 1] = '\0';
            break;
        }
    }
}

static void compute_default_root(char *out, size_t out_sz) {
    char exe[1024] = {0};
    if (platform_get_executable_path(exe, sizeof(exe)) != 0) {
        snprintf(out, out_sz, "%s", ASSET_ROOT_REL);
        return;
    }
    path_dirname_inplace(exe); /* /path/to/binary/bin */
    path_dirname_inplace(exe); /* /path/to/binary     */
    path_dirname_inplace(exe); /* /path/to            */

#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    /* /path/to + /content */
    snprintf(out, out_sz, "%s%ccontent", exe, sep);
}

static int is_abs_path(const char *p) {
    if (!p || !*p) return 0;
#if defined(_WIN32)
    return ((('A' <= p[0] && p[0] <= 'Z') || ('a' <= p[0] && p[0] <= 'z')) && p[1] == ':' && (p[2] == '\\' || p[2] ==
        '/')) || (p[0] == '\\' && p[1] == '\\');
#else
    return p[0] == '/';
#endif
}

void asset_init(const char *root_override) {
    if (root_override && *root_override) {
        if (is_abs_path(root_override)) {
            strncpy(g_asset_root, root_override, sizeof(g_asset_root) - 1);
        } else {
            char base[512] = {0};
            compute_default_root(base, sizeof(base));
#if defined(_WIN32)
            const char sep = '\\';
#else
            const char sep = '/';
#endif
            snprintf(g_asset_root, sizeof(g_asset_root), "%s%c%s", base, sep, root_override);
        }
    } else {
        compute_default_root(g_asset_root, sizeof(g_asset_root)); /* => .../content */
    }
    INFO("asset root = %s", g_asset_root);
}

const char *asset_resolve_path(const char *relpath) {
    static char buf[1024];
    const char *rp = relpath ? relpath : "";

    if (strstr(rp, "..")) return NULL;

#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif

    const char *prefix = ASSET_SUBDIR "/";
    int needs_prefix = strncmp(rp, prefix, strlen(prefix)) != 0;

    if (needs_prefix) {
        snprintf(buf, sizeof(buf), "%s%c%s%s", g_asset_root, sep, prefix, rp);
    } else {
        snprintf(buf, sizeof(buf), "%s%c%s", g_asset_root, sep, rp);
    }

    return buf;
}

int asset_read_all(const char *relpath, char **out_buf, size_t *out_size) {
    const char *abs = asset_resolve_path(relpath);
    if (!abs) return MARU_ERR_INVALID;
    return fs_read_all(abs, out_buf, out_size);
}
