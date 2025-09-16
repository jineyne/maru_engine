#include "config.h"

#include <stdlib.h>

int config_load(const char *engine_json, maru_config_t *out) {
    char *buf = NULL;
    size_t sz = 0;
    if (fs_read_all(engine_json, &buf, &sz) != MARU_OK) {
        return MARU_ERR_IO;
    }

    json_value_t *root = json_parse(buf);
    free(buf);
    if (!root) return MARU_ERR_PARSE;

    out->graphics_backend = json_get_string(root, "graphics.backend", "gl");
    out->audio_backend = json_get_string(root, "audio.backend", "al");
    out->plugin_paths = json_get_string(root, "plugin_paths", NULL);

    json_free(root);
    return MARU_OK;
}
