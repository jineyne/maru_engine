#include "mem_diag.h"

#ifdef MARU_ENABLE_MEM_DIAG

#include <stdio.h>

typedef struct {
    void *ptr;
    size_t size;
    const char *file;
    int line;
} alloc_info_t;

static alloc_info_t g_allocs[8192];
static int g_count = 0;

void *mem_alloc(size_t sz, const char *file, int line) {
    void *p = malloc(sz);
    if (p && g_count < (int) (sizeof(g_allocs) / sizeof(g_allocs[0]))) {
        g_allocs[g_count++] = (alloc_info_t){p, sz, file, line};
    }
    return p;
}

void *mem_calloc(size_t n, size_t sz, const char *file, int line) {
    void *p = calloc(n, sz);
    if (p && g_count < (int) (sizeof(g_allocs) / sizeof(g_allocs[0]))) {
        g_allocs[g_count++] = (alloc_info_t){p, n * sz, file, line};
    }
    return p;
}

void *mem_realloc(void *old_p, size_t sz, const char *file, int line) {
    void *p = realloc(old_p, sz);
    if (p && old_p != p) {
        // Remove old pointer
        for (int i = 0; i < g_count; ++i) {
            if (g_allocs[i].ptr == old_p) {
                g_allocs[i] = g_allocs[--g_count];
                break;
            }
        }
        // Add new pointer
        if (g_count < (int) (sizeof(g_allocs) / sizeof(g_allocs[0]))) {
            g_allocs[g_count++] = (alloc_info_t){p, sz, file, line};
        }
    }
    return p;
}

void mem_free(void *p) {
    if (!p) return;
    for (int i = 0; i < g_count; ++i) {
        if (g_allocs[i].ptr == p) {
            g_allocs[i] = g_allocs[--g_count];
            break;
        }
    }
    free(p);
}

void mem_dump_leaks(void) {
    if (g_count == 0) {
        printf("[mem] no leaks.\n");
        return;
    }
    printf("[mem] leaks: %d\n", g_count);
    for (int i = 0; i < g_count; ++i) {
        printf("  leak #%d: %p, %zu bytes @ %s:%d\n",
               i, g_allocs[i].ptr, g_allocs[i].size, g_allocs[i].file, g_allocs[i].line);
    }
}

#endif /* MARU_ENABLE_MEM_DIAG */
