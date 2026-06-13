#version 450

#extension GL_GOOGLE_include_directive : require
#include "../common/scene_data.glsl"

layout(set = 1, binding = 0) uniform sampler2DArray u_block_textures;

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;
layout(location = 2) flat in int v_texture_id;

layout(location = 0) out vec4 out_color;


void main() {
    float light_value = max(dot(v_normal, u_scene_data.sunlight_direction.xyz), 0.1);
    vec4 base_color = texture(u_block_textures, vec3(v_uv, v_texture_id));

    vec3 lit_color = base_color.rgb * light_value * u_scene_data.sunlight_color.w + base_color.rgb * u_scene_data.ambient_color.rgb;
    out_color = vec4(lit_color, base_color.a);
}
