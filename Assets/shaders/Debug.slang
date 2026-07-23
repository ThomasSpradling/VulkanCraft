import Math;
import Resources;

struct VertexInput {
    float4 position;
    float4 color;
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color;
};

struct PushConstantData {
    float4x4 transform;
    ImmutablePtr<VertexInput> vertex_buffer;
};
[[vk::push_constant]] ConstantBuffer<PushConstantData> g_push_constant;

[shader("vertex")]
VertexOutput main_vert(uint vertex_id : SV_VertexID) {
    VertexInput input = g_push_constant.vertex_buffer[vertex_id];
    VertexOutput output;

    float4 position = mul(input.position, g_push_constant.transform);
    output.position = position;
    output.color = input.color;

    return output;
}

[shader("fragment")]
float4 main_frag(VertexOutput frag) : SV_Target {
    return frag.color;
}
