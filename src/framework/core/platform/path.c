#include "path.h"

#if defined(_WIN32)
#include <windows.h>

int platform_get_executable_path(char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) return -1;

    DWORD len = GetModuleFileNameA(NULL, buf, (DWORD)bufsize);
    if (len == 0 || len == bufsize) return -1;

    return 0;
}

#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>

int platform_get_executable_path(char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) return -1;

    uint32_t sz = (uint32_t)bufsize;
    if (_NSGetExecutablePath(buf, &sz) != 0) {
        return -1;
    }

    char tmp[PATH_MAX];
    if (realpath(buf, tmp) == NULL) return -1;

    strncpy(buf, tmp, bufsize - 1);
    buf[bufsize - 1] = '\0';
    return 0;
}

#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>

int platform_get_executable_path(char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) return -1;

    ssize_t len = readlink("/proc/self/exe", buf, bufsize - 1);
    if (len == -1 || len >= (ssize_t)bufsize) return -1;

    buf[len] = '\0';
    return 0;
}

#else
int platform_get_executable_path(char *buf, size_t bufsize) {
    (void) buf; (void) bufsize;
    return -1;
}
#endif
