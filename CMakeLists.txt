cmake_minimum_required(VERSION 3.25)
project(vulkan_craft LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dev
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Assets Paths
add_compile_definitions(ASSET_PATH="${CMAKE_SOURCE_DIR}/Assets")


# Dependencies

## Vulkan
find_package(Vulkan REQUIRED)

## GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw SYSTEM)

## STB
# target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)

## Checksum generation
add_library(crc32 third_party/crc32/crc32.cpp)
target_include_directories(crc32 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/crc32)


## tinygltf

set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL_VENDOR OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/tinygltf SYSTEM)

## GLM
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glm SYSTEM)

## Volk
add_subdirectory(third_party/volk SYSTEM)
if (WIN32)
    target_compile_definitions(volk PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()

## ImGui
set(IMGUI_SRCS
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_demo.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_vulkan.cpp
)
add_library(imgui STATIC
    ${IMGUI_SRCS}
)
target_include_directories(imgui PUBLIC
    third_party/imgui
)
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
target_link_libraries(imgui volk glfw)

## Vulkan Memory Allocator
add_subdirectory(third_party/VulkanMemoryAllocator SYSTEM)

# Source

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
add_executable(${PROJECT_NAME} ${CORE_SOURCES})
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    add_compile_options(
        "$<$<CONFIG:Debug>:/Od>"      # disable optimization
        "$<$<CONFIG:Debug>:/Ob0>"     # disable inline expansion
        "$<$<CONFIG:Debug>:/Oy->"     # keep frame pointers
        "$<$<CONFIG:Debug>:/Z7>"
    )

    add_link_options(
        "$<$<CONFIG:Debug>:/DEBUG:FULL>"
        "$<$<CONFIG:Debug>:/INCREMENTAL>"
        "$<$<CONFIG:Debug>:/OPT:NOREF>"
        "$<$<CONFIG:Debug>:/OPT:NOICF>"
    )

elseif(MSVC)
    add_compile_options(
        "$<$<CONFIG:Debug>:/Od>"
        "$<$<CONFIG:Debug>:/Ob0>"
        "$<$<CONFIG:Debug>:/Oy->"
        "$<$<CONFIG:Debug>:/Zi>"
        "$<$<CONFIG:Debug>:/RTC1>"
    )

    add_link_options(
        "$<$<CONFIG:Debug>:/DEBUG:FULL>"
        "$<$<CONFIG:Debug>:/INCREMENTAL>"
        "$<$<CONFIG:Debug>:/OPT:NOREF>"
        "$<$<CONFIG:Debug>:/OPT:NOICF>"
    )

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        "$<$<CONFIG:Debug>:-O0>"
        "$<$<CONFIG:Debug>:-g3>"
        "$<$<CONFIG:Debug>:-fno-omit-frame-pointer>"
        "$<$<CONFIG:Debug>:-fno-inline>"
    )
endif()
# target_compile_options(${PROJECT_NAME} PRIVATE
#     $<$<CONFIG:Debug>:
#         $<$<CXX_COMPILER_ID:GNU,Clang>:-g>
#         $<$<CXX_COMPILER_ID:MSVC>: /Zi>
#     >
#     $<$<CONFIG:RelWithDebInfo>:
#         $<$<CXX_COMPILER_ID:GNU,Clang>:-O2 -gline-tables-only -fdebug-types-section>
#         $<$<CXX_COMPILER_ID:MSVC>:/O2 /Zi /Oy->
#     >
# )

# target_link_options(${PROJECT_NAME} PRIVATE
#     $<$<CONFIG:RelWithDebInfo>:
#         $<$<CXX_COMPILER_ID:MSVC>:/DEBUG /OPT:REF /OPT:ICF>
#     >
# )

target_link_libraries(${PROJECT_NAME}
    glfw
    glm::glm
    volk::volk
    VulkanMemoryAllocator
    tinygltf
    imgui
    crc32 
    ws2_32 # Windows Sockets
)