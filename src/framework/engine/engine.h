#ifndef MARU_ENGINE_H
#define MARU_ENGINE_H

#include "core.h"
#include "module.h"

typedef struct renderer renderer_t;

/* Engine lifecycle */
int maru_engine_init(const char *config_path);
bool maru_engine_tick();
void maru_engine_shutdown(void);

/* Renderer access for user */
extern renderer_t g_renderer;

#endif /* MARU_ENGINE_H */
