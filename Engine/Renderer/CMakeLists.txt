add_library(engine_renderer STATIC
    BindlessDescriptorTable.cpp
    DebugRenderer.cpp
    ImGuiRenderer.cpp
    Renderer.cpp
    Renderer2D.cpp
)

target_link_libraries(engine_renderer PUBLIC
    engine_options
    engine_includes
    engine_definitions
    platform_graphics
    platform_window
    engine_asset_manager
    engine_world
    imgui
    slang::slang
)

if (ENGINE_ENABLE_TRACY_PROFILING)
    target_link_libraries(engine_renderer PUBLIC TracyClient)
endif()
