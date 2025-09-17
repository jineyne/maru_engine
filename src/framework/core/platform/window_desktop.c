#if !defined(ANDROID) && !defined(__ANDROID__) && !defined(MARU_PLATFORM_IOS)

#include "window.h"

#include <stdlib.h>
#include <stdint.h>

#include <GLFW/glfw3.h>

#ifdef _WIN32
#   define GLFW_EXPOSE_NATIVE_WIN32
#   include <GLFW/glfw3native.h> /* glfwGetWin32Window */
#elif defined(__APPLE__)
#   define GLFW_EXPOSE_NATIVE_COCOA
#   include <GLFW/glfw3native.h> /* glfwGetCocoaWindow */
#else
#   define GLFW_EXPOSE_NATIVE_X11
#   include <GLFW/glfw3native.h> /* glfwGetX11Window, glfwGetX11Display */
#endif

platform_window_t *platform_window_create(void *native_handle, int width, int height, int vsync) {
    UNUSED(native_handle);

    if (!glfwInit()) {
        FATAL("GLFW init failed");
        return NULL;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    if (width <= 0) width = 1024;
    if (height <= 0) height = 768;

    GLFWwindow *win = glfwCreateWindow(width, height, "Maru", NULL, NULL);
    if (!win) {
        FATAL("GLFW window create failed");
        glfwTerminate();
        return NULL;
    }

    platform_window_t *pw = (platform_window_t*) calloc(1, sizeof(platform_window_t));
    pw->width = width;
    pw->height = height;
    pw->vsync = vsync ? 1 : 0;

#ifdef _WIN32
    pw->handle = (void*) glfwGetWin32Window(win); /* HWND */
#elif defined(__APPLE__)
    pw->handle = glfwGetCocoaWindow(s_glfw);                  /* NSWindow* */
#else
    pw->handle = (void*)(uintptr_t)glfwGetX11Window(s_glfw);  /* ::Window */
#endif
    pw->impl = (void*) win;

    INFO("platform_window_create: desktop GLFW window %dx%d vsync=%d", width, height, pw->vsync);
    return pw;
}

void platform_window_destroy(platform_window_t *pw) {
    if (!pw) return;
    if (pw->handle) {
        glfwDestroyWindow((GLFWwindow*) pw->impl);
    }
    free(pw);
    glfwTerminate();
}

void platform_poll_events() {
    glfwPollEvents();
}

int platform_should_close(platform_window_t *pw) {
    return glfwWindowShouldClose((GLFWwindow*) pw->impl);
}

void platform_window_get_size(platform_window_t *pw, int *out_w, int *out_h) {
    if (!pw) return;

    int w = 0, h = 0;
    glfwGetFramebufferSize((GLFWwindow*) pw->impl, &w, &h);
    if (out_w) {
        *out_w = w;
    }
    if (out_h) {
        *out_h = h;
    }

    pw->width = w;
    pw->height = h;
}

#endif
