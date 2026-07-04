add_library(engine_renderer STATIC
    DebugRenderer.cpp
    Renderer.cpp
    Renderer2D.cpp
)

target_link_libraries(engine_renderer PUBLIC
    engine_options
    engine_includes
    engine_definitions
    
    platform_graphics
    platform_window
    imgui
    tinygltf
)
