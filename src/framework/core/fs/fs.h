#ifndef MARU_FS_H
#define MARU_FS_H

#include <stddef.h>
#include "error.h"

int fs_read_all(const char *path, char **out_buf, size_t *out_size);

#endif /* MARU_FS_H */