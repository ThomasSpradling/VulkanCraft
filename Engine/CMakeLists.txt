# -- SOURCES -- #

add_subdirectory(Platform/Window)
add_subdirectory(Platform/Graphics)
add_subdirectory(Platform/Sockets)

add_subdirectory(Network)

add_subdirectory(AssetManager)
add_subdirectory(World)

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

