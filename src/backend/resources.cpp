#include "resources.h"
#include <fstream>
#include <iostream>
#include <vector>

VkShaderModule LoadShader(VkDevice device, const std::string &file_path) {
    std::ifstream file(file_path);
    if (!file.is_open())
        std::cerr << "Failed to load shader at path '" << file_path << "'.\n";

    uint32_t *code{};
    size_t file_size = static_cast<size_t>(file.tellg());
    if (file_size % 4 != 0)
        std::cerr << "Shadeer file not byte-aligned. Are you shure this is in SPIR-V format?\n";

    std::vector<char> source_buffer(file_size);

    file.seekg(0);
    file.read(source_buffer.data(), file_size);
    file.close();
    
    VkShaderModuleCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = source_buffer.size(),
        .pCode = reinterpret_cast<const uint32_t *>(source_buffer.data())
    };
    
    VkShaderModule shader_module;
    vkCreateShaderModule(device, &create_info, nullptr, &shader_module);
    return shader_module;
}
