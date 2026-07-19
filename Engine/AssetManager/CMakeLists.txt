add_library(engine_asset_manager STATIC
    AssetManager.cpp
    GLTFModel.cpp
    Mesh.cpp
    Shape.cpp
)

target_link_libraries(engine_asset_manager PUBLIC
    engine_options
    engine_includes
    engine_definitions
    platform_graphics
    tinygltf
    mikktspace
)
