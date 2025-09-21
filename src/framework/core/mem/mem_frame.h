#ifndef MARU_MEM_FRAME_H
#define MARU_MEM_FRAME_H

#include "core.h"
#include "macro.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int frame_arena_init(size_t bytes, int buffers);
void frame_arena_shutdown(void);

void frame_arena_begin(int frame_index);

void frame_arena_reset(void);

void *frame_alloc(size_t size, size_t align);

char *frame_strdup(const char *s);
char *frame_strndup(const char *s, size_t n);
char *frame_printf(const char *fmt, ...);

size_t frame_bytes_used(void);
size_t frame_bytes_capacity(void);
int frame_current_buffer(void);

#ifdef __cplusplus
}
#endif

#endif /* MARU_MEM_FRAME_H */
