#ifndef CORE_WINDOW_H
#define CORE_WINDOW_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

typedef struct platform_window {
    void *handle; /* HWND / NSWindow* / ANativeWindow* / X11 Window / etc */
    void *impl;
    int width;
    int height;
    int vsync; /* 0 or 1 */
} platform_window_t;

platform_window_t *platform_window_create(void *native_handle, int width, int height, int vsync);
void platform_window_destroy(platform_window_t *pw);

void platform_poll_events(void);
int platform_should_close(platform_window_t *pw);

void platform_window_get_size(platform_window_t *pw, int *out_w, int *out_h);

#ifdef __cplusplus
}
#endif

#endif /* CORE_WINDOW_H */
