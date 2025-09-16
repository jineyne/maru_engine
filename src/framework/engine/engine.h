#ifndef MARU_ENGINE_H
#define MARU_ENGINE_H

#include "core.h"
#include "module.h"

int maru_engine_init(const char *config_path);
bool maru_engine_tick();
void maru_engine_shutdown(void);

#endif /* MARU_ENGINE_H */
