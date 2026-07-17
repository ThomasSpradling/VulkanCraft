add_library(engine_world STATIC
    DefaultComponents.cpp
    World.cpp
)

target_link_libraries(engine_world PUBLIC
    engine_options
    engine_includes
    engine_definitions
    glm::glm
    engine_asset_manager
)
