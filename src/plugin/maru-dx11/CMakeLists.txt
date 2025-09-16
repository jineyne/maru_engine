project(maru-dx11)
add_library(maru-dx11 SHARED
    "maru_dx11.c"
    "rhi/rhi_dx11.c")

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "maru-dx11")

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE maru d3d11 dxgi dxguid uuid)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${MARU_INSTALL_BIN_DIR}
    LIBRARY DESTINATION ${MARU_INSTALL_BIN_DIR})