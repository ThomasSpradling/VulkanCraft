add_library(platform_sockets STATIC
    NetworkAddress.cpp
    NetworkBuffer.cpp
    Socket.cpp
    SocketAPI.cpp
)

target_link_libraries(platform_sockets PUBLIC
    engine_options
    engine_includes
    engine_definitions
    
    glm
)

if (WIN32)
    target_link_libraries(platform_sockets PUBLIC ws2_32)
endif()
