import SceneData;
import GLTFMaterial;
import Resources;

struct VertexInput {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv0;
    float2 uv1;
    float4 color;
    uint2 joints0;
    float4 weights0;
};

uint4 UnpackJoints(uint2 packed_joints) {
    return uint4(
        packed_joints.x & 0xffffu,
        packed_joints.x >> 16,
        packed_joints.y & 0xffffu,
        packed_joints.y >> 16
    );
}

struct VertexOutput {
    float4 position : SV_Position;
    float3 normal;
    float4 tangent;
    float2 uv0;
    float2 uv1;
    float4 color;
};

struct PushConstantData {
    float4x4 model;
    ImmutablePtr<VertexInput> vertex_buffer;
    ImmutablePtr<SceneData> scene_data;
    ImmutablePtr<GLTFMaterial> materials;

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

    output.position = position;
    output.normal = normalize(mul(float4(input.normal, 0.0), g_push_constant.model).xyz);;
    output.tangent = input.tangent;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    output.color = input.color;

    return output;
}

[shader("fragment")]
float4 main_frag(VertexOutput frag) : SV_Target {
    if (WIREFRAME)
        return float4(0.9f, 0.9f, 0.9f, 1.0f);
    
    ImmutablePtr<SceneData> scene = g_push_constant.scene_data;
    GLTFMaterial material = g_push_constant.materials[g_push_constant.material_id];

    // float3 color = frag.color.rgb;
    float3 texture_color = SampleTexture2D(material.albedo_texture, material.albedo_sampler, frag.uv0).rgb;
    // float3 color = 0.5f * texture_color + 0.5f * frag.color.rgb;
    float3 color = texture_color;

    float power = scene->sun_direction.w;
    float3 normal = normalize(frag.normal);
    float3 light_dir = normalize(-scene->sun_direction.xyz);

    return float4(color, 1.0f);

    // float3 ambient = color * scene->ambient;
    // float3 diffuse = color * max(dot(light_dir, normal), 0) * scene->sun_direction.w;
    // return float4(ambient + diffuse, frag.color.a);
}
