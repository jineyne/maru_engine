#ifndef MARU_CONFIG_H
#define MARU_CONFIG_H

#include "core.h"

typedef struct maru_config {
    const char *graphics_backend; /* "gl", "gles", "metal" */
    const char *audio_backend;    /* "al", "av" */
    const char *plugin_paths;     /* optional override */
} maru_config_t;

int config_load(const char *engine_json, maru_config_t *out);

#endif /* MARU_CONFIG_H */