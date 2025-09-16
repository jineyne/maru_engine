#include "engine.h"

int main(void) {
    if (maru_engine_init("../../testbed/config/engine.json") != 0) {
        return 1;
    }

    INFO("hello from testbed");

    maru_engine_shutdown();

    return 0;
}