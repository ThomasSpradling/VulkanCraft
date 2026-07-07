#pragma once

#include "Common.h"
#include <filesystem>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <slang/slang-com-helper.h>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "Platform/Graphics/Common.h"
#include "ShaderReflection.h"

struct CompiledShader {
    std::string module_name;

    std::unordered_map<ShaderStage, std::vector<std::string>> entry_points;

    ShaderReflection *shader_reflection = nullptr;
    Slang::ComPtr<slang::IComponentType> linked_program;
    slang::ProgramLayout *program_layout = nullptr;

    std::vector<uint32_t> spirv_code;
};

class ShaderCompiler {
public:
    ShaderCompiler(const std::filesystem::path &shader_search_path, std::span<const slang::PreprocessorMacroDesc> macros = {});
    std::optional<CompiledShader> Compile(const std::string &module_name) const;
    void Reflect(CompiledShader &shader);
private:
    Slang::ComPtr<slang::IGlobalSession> m_global_session = nullptr;
    Slang::ComPtr<slang::ISession> m_session = nullptr;
};
