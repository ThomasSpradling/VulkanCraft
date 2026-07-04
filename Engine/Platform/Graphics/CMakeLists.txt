add_library(platform_graphics STATIC
    Common.cpp
    DescriptorAllocator.cpp
    DescriptorLayoutBuilder.cpp
    DescriptorWriter.cpp
    PipelineBuilder.cpp
    ShaderModule.cpp
    VulkanBuffer.cpp
    VulkanDevice.cpp
    VulkanImage.cpp
    VulkanSwapChain.cpp
)

target_link_libraries(platform_graphics PUBLIC
    engine_options
    engine_includes
    engine_definitions
    
    platform_window
    glm
    volk::volk
    VulkanMemoryAllocator
)
