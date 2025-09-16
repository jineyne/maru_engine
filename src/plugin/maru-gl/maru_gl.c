#include "maru_gl.h"

#include "log.h"

int maru_plugin_init(void) {
    INFO("maru-gl init (OpenGL 4.6)");
    return 0;
}

void maru_plugin_shutdown(void) {
    INFO("maru-gl shutdown");
}