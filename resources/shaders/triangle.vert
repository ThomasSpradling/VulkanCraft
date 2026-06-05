#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec3 v_color;

// const vec2 positions[3] = vec2[3](
//     vec2( 0.0, -0.8),
//     vec2(-0.8,  0.8),
//     vec2( 0.8,  0.8)
// );
// const vec3 colors[3] = vec3[3](
//     vec3(1.0, 0.0, 0.0),
//     vec3(0.0, 1.0, 0.0),
//     vec3(0.0, 0.0, 1.0)
// );

void main() {
    vec2 pos = in_pos;
    gl_Position = vec4(in_pos, 0.0, 1.0);

    v_color = in_color;
}