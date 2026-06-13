#include "../common/scene_data.glsl"

layout(set = 1, binding = 0) uniform GLTFMaterialData {
    vec4 color_factors;
    vec4 metallic_roughness_factors;
} u_material_data;
