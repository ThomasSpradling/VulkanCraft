add_library(engine_includes INTERFACE)
target_include_directories(engine_includes INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(engine_definitions INTERFACE)
target_compile_definitions(engine_definitions INTERFACE
    ASSET_PATH="${CMAKE_SOURCE_DIR}/Assets"
)

# -- VALIDATION LAYERS -- #

option(ENGINE_ENABLE_VALIDATION_LAYERS "Enable Validation Layers" ON)
option(ENGINE_ENABLE_GPU_ASSISTED "Enable GPU Assisted Feature" OFF)

target_compile_definitions(engine_definitions INTERFACE
    ENGINE_ENABLE_VALIDATION_LAYERS=$<BOOL:${ENGINE_ENABLE_VALIDATION_LAYERS}>
    ENGINE_ENABLE_GPU_ASSISTED=$<BOOL:${ENGINE_ENABLE_GPU_ASSISTED}>
)


add_subdirectory(Platform/Window)
add_subdirectory(Platform/Graphics)
add_subdirectory(Platform/Sockets)

add_subdirectory(Network)
add_subdirectory(Renderer)

add_subdirectory(Application/Client)
add_subdirectory(Application/Server)

add_library(engine_client INTERFACE)
target_link_libraries(engine_client INTERFACE
    engine_options
    engine_includes
    engine_definitions
    application_client
)

add_library(engine_server INTERFACE)
target_link_libraries(engine_server INTERFACE
    engine_options
    engine_includes
    engine_definitions
    application_server
)

