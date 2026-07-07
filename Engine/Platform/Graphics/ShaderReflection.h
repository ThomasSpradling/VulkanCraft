#pragma once

#include <vector>

struct CompiledShader;

struct InputBinding {

};

struct ShaderReflection {
    std::vector<InputBinding> bindings;
};

ShaderReflection ReflectShader(const CompiledShader &shader);
