#include "engine.h"

int main(void) {
    if (maru_engine_init("../../testbed/config/engine.json") != 0) {
        return 1;
    }

    while (maru_engine_tick()) {}

    maru_engine_shutdown();

    return 0;
}