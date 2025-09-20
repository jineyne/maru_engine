#ifndef MARU_FS_H
#define MARU_FS_H

#include <stddef.h>
#include "error.h"

int fs_get_size(const char *path, size_t *out_size);

int fs_read_into(const char *path, void **out_buf, size_t cap, size_t *out_size, int need_null_terminator);


#endif /* MARU_FS_H */
