#include "fs.h"

#include <stdio.h>
#include <stdlib.h>

int fs_read_all(const char *path, char **out_buf, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return MARU_ERR_IO;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return MARU_ERR_IO;
    }

    size_t rd = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[rd] = '\0';

    if (out_buf) *out_buf = buf;
    if (out_size) *out_size = rd;

    return MARU_OK;
}