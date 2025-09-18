#ifndef MARU_FS_PATH_H
#define MARU_FS_PATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char path_sep(void);
int  path_is_abs(const char* p);
int  path_has_parent_ref(const char* p);
void path_dirname_inplace(char* p);
void path_join2(char* out, size_t out_sz, const char* a, const char* b);
void path_join3(char* out, size_t out_sz, const char* a, const char* b, const char* c);
void path_normalize_inplace(char* p);

#ifdef __cplusplus
}
#endif
#endif /* MARU_FS_PATH_H */