add_library(maru-gles SHARED
    maru_gles.c
)

set_target_properties(maru-gles PROPERTIES OUTPUT_NAME "maru-gles")

target_include_directories(maru-gles PRIVATE ${CMAKE_SOURCE_DIR}/src/framework/core)

target_link_libraries(maru-gles PRIVATE maru)

install(TARGETS maru-gles RUNTIME DESTINATION ${MARU_INSTALL_BIN_DIR}
                         LIBRARY DESTINATION ${MARU_INSTALL_BIN_DIR})