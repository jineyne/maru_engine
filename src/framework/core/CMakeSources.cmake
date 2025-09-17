set(CORE_MISC_SRCS
    "core/misc/cjson.c")

set(CORE_PLATFORM_SRCS
    "core/platform/window_desktop.c")

set(CORE_PLUGIN_SRCS
    "core/plugin/plugin.c")

set(CORE_THREAD_SRCS
    "core/thread/mutex.c")

set(CORE_SOURCES
    ${CORE_MISC_SRCS}
    ${CORE_PLATFORM_SRCS}
    ${CORE_PLUGIN_SRCS}
    ${CORE_THREAD_SRCS}

    "core/log.c"
    "core/error.c"
    "core/fs.c"
    "core/json.c"
    "core/time.c"
)