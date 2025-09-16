add_subdirectory(framework)

# ThirdParties
add_subdirectory(thirdparty/glad)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory (thirdparty/glfw)

# Plugins
add_subdirectory(plugin/maru-gl)
add_subdirectory(plugin/maru-gles)
add_subdirectory(plugin/maru-dx11)
add_subdirectory(plugin/maru-al)
# add_subdirectory(plugin/maru-av)
