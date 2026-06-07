#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "common.glsl"

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_color;
layout(location = 2) out vec2 v_uv;

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
    mat4 u_model_matrix;
    VertexBuffer u_vertex_buffer;
};

void main() {
    Vertex v = u_vertex_buffer.vertices[gl_VertexIndex];
    vec4 position = vec4(v.position, 1.0);

    gl_Position = u_scene_data.view_projection * u_model_matrix * position;
    
    v_normal = (u_model_matrix * vec4(v.normal, 0.0)).xyz;
    v_color = v.color.xyz * u_material_data.color_factors.xyz;
    v_uv.x = v.uv_x;
    v_uv.y = v.uv_y;
}
