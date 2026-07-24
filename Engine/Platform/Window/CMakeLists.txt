add_library(platform_window STATIC
    InputHandler.cpp
    Window.cpp
)

target_link_libraries(platform_window PUBLIC
    engine_options
    engine_includes
    engine_definitions

    glm
    glfw
    imgui
)

if (ENGINE_ENABLE_TRACY_PROFILING)
    target_link_libraries(platform_window PUBLIC TracyClient)
endif()
