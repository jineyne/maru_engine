add_library(maru-gl SHARED
    "maru_gl.c"
    "rhi/rhi_gl.c" "rhi/rhi_gl.h")

set_target_properties(maru-gl PROPERTIES OUTPUT_NAME "maru-gl")

target_include_directories(maru-gl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/framework/core
)

target_link_libraries(maru-gl PRIVATE maru glad opengl32)

install(TARGETS maru-gl RUNTIME DESTINATION ${MARU_INSTALL_BIN_DIR}
                        LIBRARY DESTINATION ${MARU_INSTALL_BIN_DIR})