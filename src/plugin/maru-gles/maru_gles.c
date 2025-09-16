#include "maru_gles.h"

#include "log.h"

int maru_plugin_init(void) {
    INFO("maru-gles init (GLES 3.2)");
    return 0;
}

void maru_plugin_shutdown(void) {
    INFO("maru-gles shutdown");
}