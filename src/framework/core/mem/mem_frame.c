#include "mem_frame.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "mem_diag.h"

#ifndef ALIGN_UP
#define ALIGN_UP(_v,_a) ( ((_a) <= 1) ? (_v) : ( ((size_t)(_v) + ((size_t)(_a)-1)) & ~((size_t)(_a)-1) ) )
#endif

#ifndef FRAME_PRINTF_TMP_CAP
#define FRAME_PRINTF_TMP_CAP (16 * 1024)
#endif

typedef struct frame_block_t {
    uint8_t *base;
    size_t cap;
    size_t offset;
} frame_block_t;

static struct {
    frame_block_t *blocks;
    int block_count;
    int cur;
    size_t bytes_each;
    int initialized;
} g_frame;

static frame_block_t *curblk(void) {
    if (!g_frame.initialized || g_frame.block_count <= 0) {
        return NULL;
    }

    return &g_frame.blocks[g_frame.cur % g_frame.block_count];
}

int frame_arena_init(size_t bytes, int buffers) {
    if (g_frame.initialized) return 0;
    if (buffers <= 0) buffers = 2;
    if (bytes < 1024) bytes = 1024;

    g_frame.blocks = (frame_block_t*) MARU_CALLOC((size_t) buffers, sizeof(frame_block_t));
    if (!g_frame.blocks) return -1;

    for (int i = 0; i < buffers; ++i) {
        g_frame.blocks[i].base = (uint8_t*) MARU_MALLOC(bytes);
        if (!g_frame.blocks[i].base) {
            for (int j = 0; j < i; ++j) {
                MARU_FREE(g_frame.blocks[j].base);
            }
            MARU_FREE(g_frame.blocks);
            memset(&g_frame, 0, sizeof(g_frame));
            return -1;
        }

        g_frame.blocks[i].cap = bytes;
        g_frame.blocks[i].offset = 0;
    }

    g_frame.block_count = buffers;
    g_frame.cur = 0;
    g_frame.bytes_each = bytes;
    g_frame.initialized = 1;
    return 0;
}

void frame_arena_shutdown(void) {
    if (!g_frame.initialized) return;

    for (int i = 0; i < g_frame.block_count; ++i) {
        if (g_frame.blocks[i].base) {
            MARU_FREE(g_frame.blocks[i].base);
            g_frame.blocks[i].base = NULL;
        }
    }

    MARU_FREE(g_frame.blocks);
    memset(&g_frame, 0, sizeof(g_frame));
}

void frame_arena_begin(int frame_index) {
    if (!g_frame.initialized) return;

    if (frame_index < 0) {
        g_frame.cur = (g_frame.cur + 1) % g_frame.block_count;
    } else {
        g_frame.cur = frame_index % g_frame.block_count;
    }

    frame_arena_reset();
}

void frame_arena_reset(void) {
    frame_block_t *b = curblk();
    if (!b) return;

#ifdef MARU_DEBUG
    memset(b->base, 0, b->cap);
#endif

    b->offset = 0;
}

void *frame_alloc(size_t size, size_t align) {
    frame_block_t *b = curblk();
    if (!b || !b->base) return NULL;

    if (align == 0) align = 1;
    size_t off_aligned = ALIGN_UP(b->offset, align);

    if (off_aligned + size > b->cap) {
        return NULL;
    }

    void *p = (void*) (b->base + off_aligned);
    b->offset = off_aligned + size;
    return p;
}

char *frame_strdup(const char *s) {
    if (!s) return NULL;

    size_t n = strlen(s);
    char *p = (char*) frame_alloc(n + 1, 1);

    if (!p) return NULL;

    memcpy(p, s, n);
    p[n] = '\0';

    return p;
}

char *frame_strndup(const char *s, size_t n) {
    if (!s) return NULL;

    size_t len = 0;
    while (len < n && s[len] != '\0') ++len;

    char *p = (char*) frame_alloc(len + 1, 1);
    if (!p) return NULL;

    memcpy(p, s, len);
    p[len] = '\0';

    return p;
}

char *frame_printf(const char *fmt, ...) {
    if (!fmt) return NULL;

    char tmp[FRAME_PRINTF_TMP_CAP];
    va_list ap;
    va_start(ap, fmt);

#if defined(_MSC_VER)
    int cnt = _vsnprintf_s(tmp, FRAME_PRINTF_TMP_CAP, _TRUNCATE, fmt, ap);
    if (cnt < 0) cnt = (int) strlen(tmp);
#else
    int cnt = vsnprintf(tmp, FRAME_PRINTF_TMP_CAP, fmt, ap);
    if (cnt < 0) { va_end(ap); return NULL; }
    if ((size_t)cnt >= FRAME_PRINTF_TMP_CAP) cnt = (int)(FRAME_PRINTF_TMP_CAP - 1);
#endif
    va_end(ap);

    char *p = (char*) frame_alloc((size_t) cnt + 1, 1);
    if (!p) return NULL;
    memcpy(p, tmp, (size_t) cnt);
    p[cnt] = '\0';
    return p;
}

size_t frame_bytes_used(void) {
    frame_block_t *b = curblk();
    return b ? b->offset : 0;
}

size_t frame_bytes_capacity(void) {
    frame_block_t *b = curblk();
    return b ? b->cap : 0;
}

int frame_current_buffer(void) {
    return g_frame.initialized ? g_frame.cur : -1;
}
