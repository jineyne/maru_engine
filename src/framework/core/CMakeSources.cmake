set(CORE_FS_SRCS
    "core/fs/fs.c"
    "core/fs/path.c")

set(CORE_HANDLE_SRCS
    "core/handle/handle_pool.c")

set(CORE_MATH_SRCS
    "core/math/math.c"
    "core/math/proj.c"
    "core/math/transform.c")

set(CORE_MEM_SRCS
    "core/mem/mem_diag.c"
    "core/mem/mem_frame.c")

set(CORE_MISC_SRCS
    "core/misc/cjson.c")

set(CORE_PLATFORM_SRCS
    "core/platform/path.c"
    "core/platform/window_desktop.c"
    "core/platform/input_desktop.c")

set(CORE_PLUGIN_SRCS
    "core/plugin/plugin.c")

set(CORE_THREAD_SRCS
    "core/thread/mutex.c")

set(CORE_SOURCES
    ${CORE_FS_SRCS}
    ${CORE_HANDLE_SRCS}
    ${CORE_MATH_SRCS}
    ${CORE_MEM_SRCS}
    ${CORE_MISC_SRCS}
    ${CORE_PLATFORM_SRCS}
    ${CORE_PLUGIN_SRCS}
    ${CORE_THREAD_SRCS}

    "core/log.c"
    "core/error.c"
    "core/json.c"
    "core/time.c"
)