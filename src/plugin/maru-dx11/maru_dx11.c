#include "maru_dx11.h"

#include "log.h"

int maru_plugin_init(void) {
    INFO("maru-al init (OpenAL)");
    return 0;
}

void maru_plugin_shutdown(void) {
    INFO("maru-al shutdown");
}