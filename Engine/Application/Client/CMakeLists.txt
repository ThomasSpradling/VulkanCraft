add_library(application_client STATIC
    ClientApplication.cpp
)

target_link_libraries(application_client PUBLIC
    engine_options
    engine_includes
    engine_definitions

    engine_network
    engine_renderer
)
