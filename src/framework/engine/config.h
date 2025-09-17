#ifndef MARU_CONFIG_H
#define MARU_CONFIG_H

#include "core.h"

typedef struct maru_config {
    const char *graphics_backend;
    const char *audio_backend;
    const char *plugin_paths;

    int gfx_width;
    int gfx_height;
    int gfx_vsync;
} maru_config_t;

int config_load(const char *engine_json, maru_config_t *out);
void config_free(maru_config_t *cfg);

#endif /* MARU_CONFIG_H */
