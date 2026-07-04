# Vulkan
find_package(Vulkan REQUIRED)

if(NOT TARGET vulkan::vulkan)
    if(NOT Vulkan_LIBRARY AND Vulkan_LIBRARIES)
        list(GET Vulkan_LIBRARIES 0 Vulkan_LIBRARY)
    endif()

    if(NOT Vulkan_INCLUDE_DIR AND Vulkan_INCLUDE_DIRS)
        list(GET Vulkan_INCLUDE_DIRS 0 Vulkan_INCLUDE_DIR)
    endif()

    if(NOT Vulkan_LIBRARY OR NOT Vulkan_INCLUDE_DIR)
        message(FATAL_ERROR "Vulkan found, but vulkan::vulkan could not be created")
    endif()

    add_library(vulkan::vulkan UNKNOWN IMPORTED GLOBAL)
    set_target_properties(vulkan::vulkan PROPERTIES
        IMPORTED_LOCATION "${Vulkan_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Vulkan_INCLUDE_DIR}"
    )
endif()


# Slang
find_package(slang CONFIG QUIET
    HINTS
        "$ENV{VULKAN_SDK}/Lib/cmake"
        "$ENV{VULKAN_SDK}/lib/cmake"
)

if(NOT TARGET slang::slang)
    find_path(SLANG_INCLUDE_DIR
        NAMES slang/slang.h
        HINTS
            "$ENV{VULKAN_SDK}/Include"
            "$ENV{VULKAN_SDK}/include"
        REQUIRED
    )

    find_library(SLANG_LIBRARY
        NAMES slang-compiler slang
        HINTS
            "$ENV{VULKAN_SDK}/Lib"
            "$ENV{VULKAN_SDK}/lib"
        REQUIRED
    )

    find_file(SLANG_COMPILER_DLL
        NAMES slang-compiler.dll slang.dll
        HINTS
            "$ENV{VULKAN_SDK}/Bin"
            "$ENV{VULKAN_SDK}/bin"
        REQUIRED
    )

    find_file(SLANG_GLSLANG_DLL
        NAMES slang-glslang.dll
        HINTS
            "$ENV{VULKAN_SDK}/Bin"
            "$ENV{VULKAN_SDK}/bin"
        REQUIRED
    )

    add_library(slang::slang SHARED IMPORTED GLOBAL)
    set_target_properties(slang::slang PROPERTIES
        IMPORTED_IMPLIB "${SLANG_LIBRARY}"
        IMPORTED_LOCATION "${SLANG_COMPILER_DLL}"
        INTERFACE_INCLUDE_DIRECTORIES "${SLANG_INCLUDE_DIR}"
    )

    message(STATUS "Found Slang runtime include: ${SLANG_INCLUDE_DIR}")
    message(STATUS "Found Slang runtime library: ${SLANG_LIBRARY}")
    message(STATUS "Found Slang compiler DLL: ${SLANG_COMPILER_DLL}")
    message(STATUS "Found Slang glslang DLL: ${SLANG_GLSLANG_DLL}")
endif()

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
