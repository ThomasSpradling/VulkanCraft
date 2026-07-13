add_library(engine_renderer STATIC
    DebugRenderer.cpp
    GLTFModel.cpp
    GPUResourceManager.cpp
    Renderer.cpp
    Renderer2D.cpp
    Camera.cpp
)

target_link_libraries(engine_renderer PUBLIC
    engine_options
    engine_includes
    engine_definitions
    platform_graphics
    platform_window
    imgui
    tinygltf
    slang::slang
    mikktspace
)
