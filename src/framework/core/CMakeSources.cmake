set(CORE_PLUGIN_SRCS
    "core/plugin/plugin.c")

set(CORE_THREAD_SRCS
    "core/thread/mutex.c")

set(CORE_SOURCES
    ${CORE_PLUGIN_SRCS}
    ${CORE_THREAD_SRCS}

    "core/log.c"
    "core/error.c"
    "core/fs.c"
    "core/json.c"
    "core/time.c"
)