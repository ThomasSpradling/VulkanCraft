add_library(platform_graphics STATIC
    CommandBuffer.cpp
    Common.cpp
    DescriptorAllocator.cpp
    DescriptorLayoutBuilder.cpp
    DescriptorWriter.cpp
    GarbageCollector.cpp
    ShaderCompiler.cpp
    ShaderReflection.cpp
    VulkanObjects.cpp
    VulkanBuffer.cpp
    VulkanDevice.cpp
    VulkanImage.cpp
    VulkanPipeline.cpp
    VulkanSwapChain.cpp
    VmaImplementation.cpp
)

target_link_libraries(platform_graphics PUBLIC
    engine_options
    engine_includes
    engine_definitions

    platform_window
    glm
    volk::volk
    GPUOpen::VulkanMemoryAllocator
)

if (ENGINE_ENABLE_TRACY_PROFILING)
    target_link_libraries(platform_graphics PUBLIC TracyClient)
endif()
