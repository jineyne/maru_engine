#ifndef MARU_ENGINE_H
#define MARU_ENGINE_H

#include "core.h"
#include "module.h"

int maru_engine_init(const char *config_path);
void maru_engine_shutdown(void);

#endif /* MARU_ENGINE_H */
