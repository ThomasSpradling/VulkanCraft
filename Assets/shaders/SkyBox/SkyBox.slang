import Resources;
import types;
import SceneData;

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

struct VertexOutput {
    float4 position : SV_Position;
    float3 world_position;
    float2 uv;
};

struct SkyboxData {
    float4x4 model;
    float4x4 view_projection;
    ImmutablePtr<VertexInput> vertex_buffer;
    uint envmap_id;
};

struct SkyBoxPushConstants {
    ImmutablePtr<SkyboxData> skybox_data;
};
[[vk::push_constant]] ConstantBuffer<SkyBoxPushConstants> g_push_constant;

[shader("vertex")]
VertexOutput main_vert(uint vertex_id : SV_VertexID) {
    ImmutablePtr<SkyboxData> data = g_push_constant.skybox_data;

    VertexOutput output;
    VertexInput input = data->vertex_buffer[vertex_id];

    float4 world_position = float4(input.position, 1.0f);
    world_position = mul(world_position, data->model);
    
    float4 position = mul(world_position, data->view_projection);

    output.world_position = world_position.rgb;
    output.position = position;
    output.uv = input.uv0;
    return output;
}

[shader("fragment")]
float4 main_frag(VertexOutput frag) : SV_Target {
    float4 color = SampleTextureCube(g_push_constant.skybox_data->envmap_id, 0, frag.world_position);
    return float4(color.rgb, 1);
}
