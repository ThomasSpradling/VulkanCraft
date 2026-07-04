add_library(engine_network STATIC
    NetworkHost.cpp
    Packet.cpp
)

target_link_libraries(engine_network PUBLIC
    engine_options
    engine_includes
    engine_definitions
    
    glm

    platform_sockets
    crc32
)
