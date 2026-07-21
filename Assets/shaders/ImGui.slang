import Math;
import Resources;

struct VertexInput {
    float x, y;
    float u, v;
    uint color; // each rgba channel is represented by a uint8_t
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color;
    float2 uv;
};

struct PushConstantData {
    float4 viewport_planes; // left, right, top, bottom
    ImmutablePtr<VertexInput> vertex_buffer;
    uint texture_id;
    uint sampler_id;
};
[[vk::push_constant]] ConstantBuffer<PushConstantData> g_push_constant;

[shader("vertex")]
VertexOutput main_vert(uint vertex_id : SV_VertexID) {
    VertexInput input = g_push_constant.vertex_buffer[vertex_id];
    VertexOutput output;

    float left = g_push_constant.viewport_planes.x;
    float right = g_push_constant.viewport_planes.y;
    float top = g_push_constant.viewport_planes.z;
    float bottom = g_push_constant.viewport_planes.w;

    float4x4 projection = Ortho(left, right, bottom, top);
    output.color = unpackUnorm4x8ToFloat(input.color);
    output.uv = float2(input.u, input.v);
    output.position = mul(float4(input.x, input.y, 0.0f, 1.0f), projection);

    return output;
}

[shader("fragment")]
float4 main_frag(VertexOutput frag) : SV_Target {
    return frag.color * SampleTexture2D(g_push_constant.texture_id, g_push_constant.sampler_id, frag.uv);
}
