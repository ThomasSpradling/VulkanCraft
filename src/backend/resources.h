#pragma once

#include <string>
#include <volk.h>

VkShaderModule LoadShader(VkDevice device, const std::string &file_path);