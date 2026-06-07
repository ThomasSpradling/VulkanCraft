#version 450

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform Color {
    vec3 u_color;
};

layout(set = 1, binding = 0) uniform sampler2D u_display_texture;

void main() {
    // frag_color = vec4(u_color, 1.0);
    // frag_color = mix(vec4(u_color, 1.0), vec4(v_color), 0.5);
    // frag_color = v_color;

    frag_color = mix(vec4(u_color, 1.0f), texture(u_display_texture, v_uv), 0.5);
}
