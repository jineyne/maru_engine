add_library(maru-gles SHARED
    "maru_gles.c"
    "rhi/rhi_gles.h"
    "rhi/rhi_gles.c")

set_target_properties(maru-gles PROPERTIES OUTPUT_NAME "maru-gles")

target_include_directories(maru-gles
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(maru-gles PRIVATE maru)

install(TARGETS maru-gles RUNTIME DESTINATION ${MARU_INSTALL_BIN_DIR}
                         LIBRARY DESTINATION ${MARU_INSTALL_BIN_DIR})