#ifndef MARU_PLATFORM_PATH_H
#define MARU_PLATFORM_PATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int platform_get_executable_path(char* buf, size_t bufsize);

#ifdef __cplusplus
}
#endif

#endif /* MARU_PLATFORM_PATH_H */