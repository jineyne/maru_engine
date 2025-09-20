#include "config.h"

#include "../core/mem/mem_diag.h"
#include <stdlib.h>
#include <string.h>

static char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char*) MARU_MALLOC(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

int config_load(const char *engine_json, maru_config_t *out) {
    if (!out) return MARU_ERR_INVALID;
    *out = (maru_config_t){0};

    size_t file_size = 0;
    if (fs_read_into(engine_json, NULL, 0, &file_size, TRUE) != MARU_OK || file_size == 0) {
        return MARU_ERR_IO;
    }

    char *buf = MARU_CALLOC(file_size + 1, sizeof(char));
    memset(buf, 0, file_size + 1);
    size_t sz = 0;
    if (fs_read_into(engine_json, &buf, file_size + 1, &sz, TRUE) != MARU_OK) {
        return MARU_ERR_IO;
    }

    json_value_t *root = json_parse(buf);
    MARU_FREE(buf);
    if (!root) return MARU_ERR_PARSE;

    out->graphics_backend = str_dup(json_get_string(root, "graphics.backend", "dx11"));
    out->audio_backend = str_dup(json_get_string(root, "audio.backend", "al"));
    out->plugin_paths = str_dup(json_get_string(root, "plugin_paths", NULL));

    out->gfx_width = json_get_int(root, "graphics.width", 1280);
    out->gfx_height = json_get_int(root, "graphics.height", 720);
    out->gfx_vsync = json_get_int(root, "graphics.vsync", 1);

    json_free(root);
    return MARU_OK;
}

void config_free(maru_config_t *cfg) {
    if (!cfg) return;
    if (cfg->graphics_backend) {
        MARU_FREE(cfg->graphics_backend);
        cfg->graphics_backend = NULL;
    }
    if (cfg->audio_backend) {
        MARU_FREE(cfg->audio_backend);
        cfg->audio_backend = NULL;
    }
    if (cfg->plugin_paths) {
        MARU_FREE(cfg->plugin_paths);
        cfg->plugin_paths = NULL;
    }
}
