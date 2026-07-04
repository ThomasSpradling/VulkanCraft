if(MSVC)
    add_compile_options(/w)
else()
    add_compile_options(-w)
endif()

find_package(Vulkan REQUIRED)

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

# tinygltf
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL_VENDOR OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(tinygltf)

# GLM
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
add_subdirectory(glm)

# Volk
add_subdirectory(volk)

if (WIN32)
    target_compile_definitions(volk PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()

# Vulkan Memory Allocator
add_subdirectory(VulkanMemoryAllocator)

# crc32
add_library(crc32 STATIC
    crc32/crc32.cpp
)

target_include_directories(crc32 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/crc32
)

# ImGui
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_demo.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

target_compile_definitions(imgui PRIVATE
    IMGUI_IMPL_VULKAN_USE_VOLK
)

target_link_libraries(imgui PUBLIC
    glfw
    volk::volk
)
