#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;
layout(location = 0) out vec4 v_color;

layout(push_constant, std430) uniform PushConstants {
    mat4 u_view_projection;
};

void main() {
    gl_Position = u_view_projection * vec4(in_position, 1.0);

    // v_color = in_color;
    v_color = vec4(in_normal, 1.0);
}