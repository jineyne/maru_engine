#ifndef MARU_ASSET_H
#define MARU_ASSET_H

#include "core.h"

#define ASSET_ROOT_REL "../../content"
#define ASSET_SUBDIR   "assets"

void asset_init(const char* root_override);
int  asset_read_all(const char* relpath, char** out_buf, size_t* out_size);

const char* asset_resolve_path(const char* relpath);

#endif /* MARU_ASSET_H */