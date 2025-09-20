#ifndef MARU_ASSET_H
#define MARU_ASSET_H

#include "core.h"

void asset_init(const char *root_override);
int asset_read_into(const char *relpath, void **out_buf, size_t cap, size_t *out_size, int need_null_terminator);

// NOTE NEED MARU_FREE() MANUALLY!!!!
char *asset_read_all(const char *relpath, size_t *out_size, int need_null_terminator);

const char *asset_resolve_path(const char *relpath);

#endif /* MARU_ASSET_H */
