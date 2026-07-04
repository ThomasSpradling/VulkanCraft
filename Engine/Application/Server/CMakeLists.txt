add_library(application_server STATIC
    ServerApplication.cpp
)

target_link_libraries(application_server PUBLIC
    engine_options
    engine_includes
    engine_definitions

    engine_network
)
