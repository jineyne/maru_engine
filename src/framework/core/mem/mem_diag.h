#ifndef MARU_MEM_DIAG
#define MARU_MEM_DIAG

#include <stdlib.h>

#define MARU_ENABLE_MEM_DIAG

#ifdef MARU_ENABLE_MEM_DIAG
void *mem_alloc(size_t sz, const char *file, int line);
void *mem_calloc(size_t n, size_t sz, const char *file, int line);
void *mem_realloc(void *p, size_t sz, const char *file, int line);
void mem_free(void *p);
void mem_dump_leaks(void);
#define MARU_MALLOC(sz) mem_alloc((sz), __FILE__, __LINE__)
#define MARU_CALLOC(n, sz) mem_calloc((n), (sz), __FILE__, __LINE__)
#define MARU_REALLOC(p, sz) mem_realloc((p), (sz), __FILE__, __LINE__)
#define MARU_FREE(p)    mem_free((p))
#else
#define MARU_MALLOC(sz) malloc((sz))
#define MARU_CALLOC(n, sz) calloc((n), (sz))
#define MARU_REALLOC(p, sz) realloc((p), (sz))
#define MARU_FREE(p)    free((p))
#endif

#endif /* MARU_MEM_DIAG */
