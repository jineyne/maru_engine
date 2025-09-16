#ifndef MARU_MODULE_H
#define MARU_MODULE_H

#include <stdint.h>

typedef struct maru_module {
    const char *name;
    int (*on_load)(void);
    void (*on_unload)(void);
} maru_module;

#endif /* MARU_MODULE_H */
