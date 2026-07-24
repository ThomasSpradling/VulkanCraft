add_library(engine_network STATIC
    NetworkHost.cpp
    Packet.cpp
)

target_link_libraries(engine_network PUBLIC
    engine_options
    engine_includes
    engine_definitions
    platform_sockets
    glm
    crc32
)

if (ENGINE_ENABLE_TRACY_PROFILING)
    target_link_libraries(engine_network PUBLIC TracyClient)
endif()
