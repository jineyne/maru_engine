#include "fs.h"

#include <stdio.h>
#include <stdlib.h>

#include "mem/mem_diag.h"

int fs_get_size(const char *path, size_t *out_size) {
    if (!path || !out_size) {
        return MARU_ERR_INVALID;
    }

    FILE *f = fopen(path, "rb");
    if (!f) return MARU_ERR_IO;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return MARU_ERR_IO;
    }

    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        return MARU_ERR_IO;
    }

    fclose(f);
    *out_size = (size_t) len;
    return MARU_OK;
}

int fs_read_into(const char *path, void **out_buf, size_t cap, size_t *out_size, int need_null_terminator) {
    if (!path) {
        return MARU_ERR_INVALID;
    }

    FILE *f = fopen(path, "rb");
    if (!f) return MARU_ERR_IO;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return MARU_ERR_IO;
    }

    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        return MARU_ERR_IO;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return MARU_ERR_IO;
    }

    size_t want = (size_t) len;
    size_t need = want + ((need_null_terminator) ? 1 : 0);
    if (out_buf == NULL) {
        if (out_size) *out_size = want;
        return MARU_OK;
    }

    if (cap < need) {
        fclose(f);
        return MARU_ERR_IO;
    }
    
    size_t rd = fread(*out_buf, 1, want, f);
    fclose(f);

    if (need_null_terminator) {
        ((unsigned char*) *out_buf)[rd] = 0;
    }

    return MARU_OK;
}
