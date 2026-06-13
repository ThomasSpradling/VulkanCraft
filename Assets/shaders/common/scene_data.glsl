layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    vec4 ambient_color;
    vec4 sunlight_direction;
    vec4 sunlight_color;
} u_scene_data;
