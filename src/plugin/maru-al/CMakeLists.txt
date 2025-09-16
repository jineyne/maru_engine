add_library(maru-al SHARED maru_al.c)

set_target_properties(maru-al PROPERTIES OUTPUT_NAME "maru-al")

target_include_directories(maru-al PRIVATE ${CMAKE_SOURCE_DIR}/src/framework/core)

target_link_libraries(maru-al PRIVATE maru)

install(TARGETS maru-al RUNTIME DESTINATION ${MARU_INSTALL_BIN_DIR}
                      LIBRARY DESTINATION ${MARU_INSTALL_BIN_DIR})