cmake_minimum_required(VERSION 3.21)
project(maru C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(MARU_BUILD_TESTBED "Build testbed projects" ON)
option(MARU_UNITY_BUILD "Unity-build framework modules" ON)

# Install layout
include(GNUInstallDirs)

set(MARU_INSTALL_BIN_DIR ${CMAKE_INSTALL_BINDIR})
set(MARU_INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR})
set(MARU_INSTALL_INC_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(MARU_INSTALL_RES_DIR ${CMAKE_INSTALL_DATADIR}/maru/resources)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/binary/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/binary/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/binary/bin)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(${CMAKE_SOURCE_DIR}/cmake/maru-unity.cmake)

add_subdirectory(src)

# Testbed
if(MARU_BUILD_TESTBED)
  add_subdirectory(testbed)
endif()

function(maru_unity_sources OUT_VAR)
    set(files ${ARGN})
    set(unity_file ${CMAKE_CURRENT_BINARY_DIR}/unity_build.c)
    file(WRITE ${unity_file} "/* Auto-generated unity build */\n")
    foreach(f IN LISTS files)
        file(APPEND ${unity_file} "#include \"${f}\"\n")
    endforeach()
    set(${OUT_VAR} ${unity_file} PARENT_SCOPE)
endfunction()