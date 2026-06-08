#version 450

#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec4 v_color;
layout(location = 2) in vec2 v_uv;

layout(location = 0) out vec4 out_color;

void main() {
    float light_value = max(dot(v_normal, u_scene_data.sunlight_direction.xyz), 0.1);
    vec4 base_color = v_color * texture(u_albedo, v_uv);

    vec3 lit_color = base_color.rgb * light_value * u_scene_data.sunlight_color.w + base_color.rgb * u_scene_data.ambient_color.rgb;
    out_color = vec4(lit_color, base_color.a);
}
