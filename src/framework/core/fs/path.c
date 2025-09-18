#include "path.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

char path_sep(void) {
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

int path_is_abs(const char *p) {
    if (!p || !*p) return 0;
#if defined(_WIN32)
    /* C:\ or \\server\share */
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':' && (p[2] == '\\' || p[2] == '/')) {
        return 1;
    }

    if (p[0] == '\\' && p[1] == '\\') {
        return 1;
    }

    return 0;
#else
    return p[0] == '/';
#endif
}

int path_has_parent_ref(const char *p) {
    return (p && strstr(p, "..") != NULL);
}

static int ends_with_sep(const char *p) {
    size_t n = p ? strlen(p) : 0;

    if (n == 0) return 0;
    char c = p[n - 1];

    return (c == '/' || c == '\\');
}

void path_dirname_inplace(char *p) {
    if (!p) return;

    size_t n = strlen(p);
    for (size_t i = n; i > 0; --i) {
        char c = p[i - 1];
        if (c == '/' || c == '\\') {
            p[i - 1] = '\0';
            return;
        }
    }

    p[0] = '\0';
}

void path_join2(char *out, size_t out_sz, const char *a, const char *b) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';

    const char sep = path_sep();
    const char *pa = a ? a : "";
    const char *pb = b ? b : "";

    if (*pa == '\0') {
        snprintf(out, out_sz, "%s", pb);
        return;
    }
    if (*pb == '\0') {
        snprintf(out, out_sz, "%s", pa);
        return;
    }

    if (ends_with_sep(pa)) {
        snprintf(out, out_sz, "%s%s", pa, (*pb == '/' || *pb == '\\') ? (pb + 1) : pb);
    } else {
        snprintf(out, out_sz, "%s%c%s", pa, sep, (*pb == '/' || *pb == '\\') ? (pb + 1) : pb);
    }
}

void path_join3(char *out, size_t out_sz, const char *a, const char *b, const char *c) {
    char tmp[1024];
    path_join2(tmp, sizeof(tmp), a, b);
    path_join2(out, out_sz, tmp, c);
}

void path_normalize_inplace(char *p) {
    if (!p) return;
#if !defined(_WIN32)

    for (char* q = p; *q; ++q) if (*q == '\\') {
        *q = '/';
    }

#endif

    char *src = p, *dst = p;
    char last = '\0';
    while (*src) {
        if ((*src == '/' || *src == '\\') && (last == '/' || last == '\\')) {
            ++src;
            continue;
        }

        *dst++ = *src;
        last = *src++;
    }
    *dst = '\0';
    size_t n = strlen(p);
    if (n > 1) {
        char c = p[n - 1];
        if (c == '/' || c == '\\') p[n - 1] = '\0';
    }
}
