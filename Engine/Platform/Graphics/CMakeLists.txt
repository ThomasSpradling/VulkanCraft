add_library(platform_graphics STATIC
    CommandBuffer.cpp
    Common.cpp
    DescriptorAllocator.cpp
    DescriptorLayoutBuilder.cpp
    DescriptorWriter.cpp
    PipelineBuilder.cpp
    ShaderCompiler.cpp
    ShaderReflection.cpp
    VulkanObjects.cpp
    VulkanBuffer.cpp
    VulkanDevice.cpp
    VulkanImage.cpp
    VulkanSwapChain.cpp
    VmaImplementation.cpp
)

target_link_libraries(platform_graphics
    engine_options
    engine_includes
    engine_definitions

    platform_window
    glm
    volk::volk
    GPUOpen::VulkanMemoryAllocator
)
