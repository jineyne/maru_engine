cmake_minimum_required(VERSION 3.21)

include(CMakeSources.cmake)

add_executable(testbed ${TESTBED_SOURCES})

target_include_directories(testbed PRIVATE
    ${CMAKE_SOURCE_DIR}/src/framework/core
    ${CMAKE_SOURCE_DIR}/src/framework/engine)

target_link_libraries(testbed PRIVATE maru)

install(TARGETS testbed RUNTIME DESTINATION ${MARU_INSTALL_BIN_DIR})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config/ DESTINATION ${MARU_INSTALL_RES_DIR}/config)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/ DESTINATION ${MARU_INSTALL_RES_DIR}/assets)