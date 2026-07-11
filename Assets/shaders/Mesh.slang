import SceneData;
import BasicMaterial;
import Global;

struct VertexInput {
    float3 position;
    float uv_x;
    float3 normal;
    float uv_y;
    float4 color;
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color;
    float3 normal;
    float2 uv;
};

struct PushConstantData {
    float4x4 model;
    ImmutablePtr<VertexInput> vertex_buffer;
    ImmutablePtr<SceneData> scene_data;
    ImmutablePtr<Material> materials;

    uint material_id;
}
[[vk::push_constant]] ConstantBuffer<PushConstantData> g_push_constant;

[[SpecializationConstant]] const bool WIREFRAME = false;

[shader("vertex")]
VertexOutput main_vert(uint vertex_id : SV_VertexID) {
    VertexInput input = g_push_constant.vertex_buffer[vertex_id];
    VertexOutput output;
    
    ImmutablePtr<SceneData> scene = g_push_constant.scene_data;

    float4 position = float4(input.position, 1.0);
    position = mul(position, g_push_constant.model);
    position = mul(position, scene->view);
    position = mul(position, scene->projection);

    output.color = input.color;
    output.position = position;
    output.normal = normalize(mul(float4(input.normal, 0.0), g_push_constant.model).xyz);;
    output.uv = float2(input.uv_x, input.uv_y);

    return output;
}

[shader("fragment")]
float4 main_frag(VertexOutput frag) : SV_Target {
    if (WIREFRAME)
        return float4(0.9f, 0.9f, 0.9f, 1.0f);
    
    ImmutablePtr<SceneData> scene = g_push_constant.scene_data;
    Material material = g_push_constant.materials[g_push_constant.material_id];

    // float3 color = frag.color.rgb;
    float3 texture_color = SampleTexture2D(material.color_texture, material.color_sampler, frag.uv).rgb;
    float3 color = 0.5f * texture_color + 0.5f * frag.color.rgb;

    float power = scene->sun_direction.w;
    float3 normal = normalize(frag.normal);
    float3 light_dir = normalize(-scene->sun_direction.xyz);

    float3 ambient = color * scene->ambient;
    float3 diffuse = color * max(dot(light_dir, normal), 0) * scene->sun_direction.w;
    return float4(ambient + diffuse, frag.color.a);
}
