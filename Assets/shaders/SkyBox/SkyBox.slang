import Resources;
import types;

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

struct SkyBoxPushConstants {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    ImmutablePtr<VertexInput> vertex_buffer;
    TextureId cube_map_id = 0;
};
[[vk::push_constant]] ConstantBuffer<SkyBoxPushConstants> g_push_constant;

[shader("vertex")]
VertexOutput main_vert(uint vertex_id : SV_VertexID) {
    VertexOutput output;
    VertexInput input = g_push_constant.vertex_buffer[vertex_id];

    float4 world_position = float4(input.position, 1.0f);
    world_position = mul(world_position, g_push_constant.model);
    
    float4 position = mul(world_position, g_push_constant.view);
    position = mul(position, g_push_constant.projection);

    output.world_position = world_position.rgb;
    output.position = position;
    output.uv = input.uv0;
    return output;
}

[shader("fragment")]
float4 main_frag(VertexOutput frag) : SV_Target {
    float4 color = SampleTextureCube(g_push_constant.cube_map_id, 0, frag.world_position);
    // return color;
    return float4(color.rgb, 1);
}
