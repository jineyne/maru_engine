#include "engine.h"
#include "engine_context.h"
#include "mem/mem_diag.h"
#include "platform/window.h"
#include "rhi/rhi.h"

#if WIN32
#include <crtdbg.h>
#endif

extern engine_context_t g_ctx;

int main(void) {
    if (maru_engine_init("../../config/engine.json") != 0) {
        return 1;
    }

    rhi_swapchain_t *sc = g_ctx.active_rhi->get_swapchain(g_ctx.active_device);

    while (maru_engine_tick()) {
    }

    maru_engine_shutdown();

    mem_dump_leaks();

#if WIN32
    _CrtDumpMemoryLeaks();
#endif

    return 0;
}
