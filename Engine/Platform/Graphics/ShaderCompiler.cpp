#include "ShaderCompiler.h"

#include "Common.h"
#include "ShaderReflection.h"
#include <array>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

ShaderCompiler::ShaderCompiler(const std::filesystem::path &shader_search_path, std::span<const slang::PreprocessorMacroDesc> macros) {
    //// Create Global Session ////
    SlangGlobalSessionDesc global_session_desc {
        .enableGLSL = true,
    };
    if (SLANG_FAILED(slang::createGlobalSession(&global_session_desc, m_global_session.writeRef())))
        throw std::runtime_error("Failed to create global Slang session!");

    //// Preprocess File Paths ////
    std::string search_path = shader_search_path.string();
    std::vector<const char *> search_paths = { search_path.c_str() };

    //// Create Session ////
    slang::TargetDesc target_desc = {
        .format = SLANG_SPIRV,
        .profile = m_global_session->findProfile("spirv_1_5"),
    };

    std::vector<slang::CompilerOptionEntry> options {
        {
            .name = slang::CompilerOptionName::EmitSpirvDirectly,
            .value = {
                .kind = slang::CompilerOptionValueKind::Int,
                .intValue0 = 1,
            },
        },
        {
            .name = slang::CompilerOptionName::VulkanUseEntryPointName,
            .value = {
                .kind = slang::CompilerOptionValueKind::Int,
                .intValue0 = 1,
            }
        },
        {
            .name = slang::CompilerOptionName::DisableWarning,
            .value = {
                .kind = slang::CompilerOptionValueKind::String,
                .stringValue0 = "41012", // profile-implicitly-upgraded
            },
        },
    };

    slang::SessionDesc session_desc = {
        .targets = &target_desc,
        .targetCount = 1,
        .searchPaths = search_paths.data(),
        .searchPathCount = static_cast<SlangInt>(search_paths.size()),
        .preprocessorMacros = macros.data(),
        .preprocessorMacroCount = static_cast<SlangInt>(macros.size()),
        .compilerOptionEntries = options.data(),
        .compilerOptionEntryCount = static_cast<uint32_t>(options.size()),
    };

    if (SLANG_FAILED(m_global_session->createSession(session_desc, m_session.writeRef())))
        throw std::runtime_error("Failed to create Slang session!");
}

std::optional<CompiledShader> ShaderCompiler::Compile(const std::string &module_name) const {
    std::cout << "Compiling shader: " << module_name << ".slang\n";

    //// Create Slang Module ////
    Slang::ComPtr<slang::IModule> slang_module = nullptr;
    {
        Slang::ComPtr<slang::IBlob> diagnostics = nullptr;
        slang_module = m_session->loadModule(module_name.c_str(), diagnostics.writeRef());
    
        if (diagnostics)
            std::cerr << static_cast<const char*>(diagnostics->getBufferPointer()) << "\n";
    
        if (!slang_module)
            throw std::runtime_error(std::format("Failed to load Slang module '{}'.", module_name));
    }

    //// Collect Entry Points ////
    CompiledShader compiled_shader {
        .module_name = module_name,
    };

    SlangInt32 entry_point_count = slang_module->getDefinedEntryPointCount();

    std::vector<Slang::ComPtr<slang::IEntryPoint>> entry_points;
    entry_points.reserve(entry_point_count);

    std::vector<slang::IComponentType *> components {};
    components.reserve(1 + entry_point_count);
    components.push_back(slang_module);

    for (SlangInt32 i = 0; i < entry_point_count; ++i) {
        Slang::ComPtr<slang::IEntryPoint> entry_point;
        SlangResult result = slang_module->getDefinedEntryPoint(i, entry_point.writeRef());

        if (SLANG_FAILED(result) || !entry_point)
            throw std::runtime_error(std::format("Failed to get entry point [{}] for Slang module '{}'.", i, module_name));
        
        components.push_back(entry_point);
        entry_points.push_back(entry_point);
    }

    //// Compose Shader Program ////
    Slang::ComPtr<slang::IComponentType> program = nullptr;
    {
        Slang::ComPtr<slang::IBlob> diagnostics = nullptr;
        SlangResult result = m_session->createCompositeComponentType(
            components.data(),
            static_cast<SlangInt32>(components.size()),
            program.writeRef(),
            diagnostics.writeRef()
        );

        if (diagnostics)
            std::cerr << static_cast<const char*>(diagnostics->getBufferPointer()) << "\n";

        if (SLANG_FAILED(result) || !program)
            throw std::runtime_error(std::format("Failed to create composite program for Slang module '{}'.", module_name));
    }

    //// Link Program ////
    Slang::ComPtr<slang::IComponentType> linked_program = nullptr;
    {
        Slang::ComPtr<slang::IBlob> diagnostics = nullptr;
        SlangResult result = program->link(linked_program.writeRef(), diagnostics.writeRef());

        if (diagnostics)
            std::cerr << static_cast<const char*>(diagnostics->getBufferPointer()) << "\n";

        if (SLANG_FAILED(result) || !linked_program)
            throw std::runtime_error(std::format("Failed to link Slang module '{}'.", module_name));
    }

    //// Get Program Layout ////
    slang::ProgramLayout *program_layout = nullptr;
    {
        Slang::ComPtr<slang::IBlob> diagnostics = nullptr;
        program_layout = linked_program->getLayout(0, diagnostics.writeRef());

        if (diagnostics)
            std::cerr << static_cast<const char*>(diagnostics->getBufferPointer()) << "\n";

        if (!program_layout)
            throw std::runtime_error(std::format("Failed to get program layout of Slang module '{}'.", module_name));
    }

    SlangUInt count = program_layout->getEntryPointCount();
    for (SlangUInt i = 0; i < count; ++i) {        
        slang::EntryPointReflection *entry_reflection = program_layout->getEntryPointByIndex(i);
        if (entry_reflection == nullptr)
            throw std::runtime_error(std::format("Could not reflect entry point [{}] of Slang module '{}'.", i, module_name));
        
        std::string entry_name = entry_reflection->getName();
        auto shader_stage = GetShaderStageFromSlang(entry_reflection->getStage());
        if (shader_stage == std::nullopt)
            throw std::runtime_error(std::format("Invalid shader stage for entry point {}() of Slang module '{}'.", entry_name, module_name));
        
        compiled_shader.entry_points[*shader_stage].emplace_back(entry_name);
    }

    //// Compile Code ////
    Slang::ComPtr<slang::IBlob> spirv_code_blob = nullptr;
    {
        Slang::ComPtr<slang::IBlob> diagnostics = nullptr;
        SlangResult result = linked_program->getTargetCode(0, spirv_code_blob.writeRef(), diagnostics.writeRef());
        if (diagnostics)
            std::cerr << static_cast<const char*>(diagnostics->getBufferPointer()) << "\n";

        if (SLANG_FAILED(result) || !spirv_code_blob)
            throw std::runtime_error(std::format("Failed to compile Slang module '{}'.", module_name));
    }

    if (spirv_code_blob->getBufferSize() == 0 || spirv_code_blob->getBufferSize() % sizeof(uint32_t) != 0)
        throw std::runtime_error(std::format("SPIR-V must be word-aligned for Slang module '{}'.", module_name));
    
    const size_t spirv_word_count = spirv_code_blob->getBufferSize() / sizeof(uint32_t);
    compiled_shader.spirv_code.resize(spirv_word_count);

    std::memcpy(compiled_shader.spirv_code.data(), spirv_code_blob->getBufferPointer(), spirv_code_blob->getBufferSize());

    compiled_shader.linked_program = linked_program;
    compiled_shader.program_layout = program_layout;

    return compiled_shader;
}

void ShaderCompiler::Reflect(CompiledShader &shader) {
    // ShaderReflection reflection = ReflectShader(shader);
    // shader.shader_reflection = &reflection;
}
