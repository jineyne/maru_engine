#include "asset.h"
#include "fs/path.h"
#include "log.h"
#include "platform/path.h"

#include <string.h>
#include <stdlib.h>


static char g_asset_root[512] = {0};

static void compute_default_root(char *out, size_t out_sz) {
    char exe[1024] = {0};
    if (platform_get_executable_path(exe, sizeof(exe)) != 0) {
        snprintf(out, out_sz, "%s", ASSET_ROOT_REL);
        return;
    }
    path_dirname_inplace(exe); /* /path/to/binary/bin */
    path_dirname_inplace(exe); /* /path/to/binary     */
    path_dirname_inplace(exe); /* /path/to            */

    char tmp[1024];
    path_join2(tmp, sizeof(tmp), exe, "content");
    snprintf(out, out_sz, "%s", tmp);
    path_normalize_inplace(out);
}

void asset_init(const char *root_override) {
    if (root_override && *root_override) {
        if (path_is_abs(root_override)) {
            snprintf(g_asset_root, sizeof(g_asset_root), "%s", root_override);
        } else {
            char base[512] = {0};
            compute_default_root(base, sizeof(base));
            path_join2(g_asset_root, sizeof(g_asset_root), base, root_override);
        }
    } else {
        compute_default_root(g_asset_root, sizeof(g_asset_root));
    }
    path_normalize_inplace(g_asset_root);
    INFO("asset root = %s", g_asset_root);
}

const char *asset_resolve_path(const char *relpath) {
    static char buf[1024];
    const char *rp = relpath ? relpath : "";

    if (path_has_parent_ref(rp)) {
        return NULL;
    }

    char assets_root[1024];
    path_join2(assets_root, sizeof(assets_root), g_asset_root, ASSET_SUBDIR);

    const char *prefix = "assets/";
    if (strncmp(rp, prefix, strlen(prefix)) == 0) {
        path_join2(buf, sizeof(buf), g_asset_root, rp);
    } else {
        path_join2(buf, sizeof(buf), assets_root, rp);
    }
    path_normalize_inplace(buf);
    return buf;
}

int asset_read_all(const char *relpath, char **out_buf, size_t *out_size) {
    const char *abs = asset_resolve_path(relpath);
    if (!abs) return MARU_ERR_INVALID;
    return fs_read_all(abs, out_buf, out_size);
}
