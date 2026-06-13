#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "../common/scene_data.glsl"

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out int v_texture_id;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    int texture_id;
    int pad0;
    int pad1;
    int pad2;
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
    
    mat3 normal_matrix = transpose(inverse(mat3(u_model_matrix)));
    v_normal = normalize(normal_matrix * v.normal);

    v_uv.x = v.uv_x;
    v_uv.y = v.uv_y;
    v_texture_id = v.texture_id;
}
