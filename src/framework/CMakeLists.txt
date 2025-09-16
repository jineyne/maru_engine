project(maru)

include(core/CMakeSources.cmake)
include(engine/CMakeSources.cmake)

add_library(${PROJECT_NAME} STATIC ${CORE_SOURCES} ${ENGINE_SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/engine)

install(TARGETS maru
        RUNTIME DESTINATION ${MARU_INSTALL_BIN_DIR}
        LIBRARY DESTINATION ${MARU_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${MARU_INSTALL_LIB_DIR})

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC MARU_PLATFORM_WINDOWS)
elseif(ANDROID)
    target_compile_definitions(${PROJECT_NAME} PUBLIC MARU_PLATFORM_ANDROID)
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC MARU_PLATFORM_IOS)
endif()

install(DIRECTORY src/framework/ DESTINATION ${MARU_INSTALL_INC_DIR}/maru
        FILES_MATCHING PATTERN "*.h")