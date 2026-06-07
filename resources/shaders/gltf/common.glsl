layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    vec4 ambient_color;
    vec4 sunlight_direction;
    vec4 sunlight_color;
} u_scene_data;

layout(set = 1, binding = 0) uniform GLTFMaterialData {
    vec4 color_factors;
    vec4 metallic_roughness_factors;
} u_material_data;

layout(set = 1, binding = 1) uniform sampler2D u_albedo;
layout(set = 1, binding = 2) uniform sampler2D u_metallic_roughness;