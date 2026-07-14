cmake_minimum_required(VERSION 3.25)

project(vulkan_craft LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(engine_options INTERFACE)
target_compile_features(engine_options INTERFACE cxx_std_23)

add_library(engine_includes INTERFACE)
target_include_directories(engine_includes INTERFACE
    ${CMAKE_SOURCE_DIR}/Engine
)

add_library(engine_definitions INTERFACE)
target_compile_definitions(engine_definitions INTERFACE
    ASSET_PATH="${CMAKE_SOURCE_DIR}/Assets"
)

# -- VALIDATION LAYERS -- #

option(ENGINE_ENABLE_VALIDATION_LAYERS "Enable Validation Layers" ON)

target_compile_definitions(engine_definitions INTERFACE
    ENGINE_ENABLE_VALIDATION_LAYERS=$<BOOL:${ENGINE_ENABLE_VALIDATION_LAYERS}>
    ENGINE_ENABLE_GPU_ASSISTED=$<BOOL:${ENGINE_ENABLE_GPU_ASSISTED}>
)

# -- WARNINGS AND ERRORS -- #

option(ENGINE_WARNINGS_AS_ERRORS "Enable Warnings as Errors" ON)
option(ENGINE_STRICT_WARNINGS "Enable Strict Warnings" ON)

if(MSVC)
    if(ENGINE_STRICT_WARNINGS)
        set(WARNINGS /W4)
    else()
        set(WARNINGS /W3)
    endif()

    if(ENGINE_WARNINGS_AS_ERRORS)
        list(APPEND WARNINGS /WX)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND WARNINGS
            /clang:-Wno-unused-parameter
            /clang:-Wno-missing-designated-field-initializers
        )
    else()
        list(APPEND WARNINGS
            /wd4100
        )
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    if(ENGINE_STRICT_WARNINGS)
        set(WARNINGS -Wall -Wextra -Wpedantic)
    else()
        set(WARNINGS -Wall)
    endif()

    if(ENGINE_WARNINGS_AS_ERRORS)
        list(APPEND WARNINGS -Werror)
    endif()

    list(APPEND WARNINGS
        -Wno-unused-parameter
        -Wno-missing-designated-field-initializers
    )
endif()

target_compile_options(engine_options INTERFACE ${WARNINGS})

# -- THIRD PARTY -- #

add_subdirectory(third_party SYSTEM)

# -- CLANG TIDY -- #

option(ENABLE_CLANG_TIDY "Run clang-tidy." OFF)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")

    if(MSVC)
        list(APPEND CMAKE_CXX_CLANG_TIDY "--extra-arg=/EHsc")
    else()
        list(APPEND CMAKE_CXX_CLANG_TIDY "--extra-arg=-fexceptions" "--extra-arg=-fcxx-exceptions")
    endif()
endif()

# -- ADDRESS SANITIZER -- #

option(ENGINE_ENABLE_ADDRESS_SANITIZER "Enable AddressSanitizer" OFF)

if(ENGINE_ENABLE_ADDRESS_SANITIZER)
    message(STATUS "AddressSanitizer enabled")

    if(MSVC)
        string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")

        target_compile_options(engine_options INTERFACE /fsanitize=address /Zi)
        target_link_options(engine_options INTERFACE /INCREMENTAL:NO)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
        target_compile_options(engine_options INTERFACE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(engine_options INTERFACE -fsanitize=address)
    else()
        message(WARNING "AddressSanitizer is not configured for compiler ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

# -- SOURCES -- #

add_subdirectory(Engine)

add_subdirectory(Games/SimpleGame)
add_subdirectory(Games/NetworkTest)
