#version 450

#extension GL_EXT_buffer_reference : require

// layout(location = 0) in vec3 in_position;
// layout(location = 1) in vec3 in_normal;
// layout(location = 2) in vec4 in_color;
// layout(location = 3) in vec2 in_uv;
layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform PushConstants {
    mat4 u_model;
    mat4 u_view_projection;
    VertexBuffer u_vertex_buffer;
};

void main() {
    Vertex v = u_vertex_buffer.vertices[gl_VertexIndex];
    gl_Position = u_view_projection * u_model * vec4(v.position, 1.0);

    // gl_Position = u_view_projection * vec4(in_position, 1.0);

    // v_color = in_color;
    v_color = vec4(v.normal, 0.5);
    v_uv = vec2(v.uv_x, v.uv_y);
}