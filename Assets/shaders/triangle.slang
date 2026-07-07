static float2 positions[3] = float2[](
    float2( 0.0,-0.5),
    float2( 0.5, 0.5),
    float2(-0.5, 0.5),
);

static float3 colors[3] = float3[](
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
);

struct VertexOutput {
    float4 v_position : SV_Position;
    float4 color;
};

[shader("vertex")]
VertexOutput main_vert(uint vertex_id : SV_VertexID) {
    VertexOutput output;
    output.v_position = float4(positions[vertex_id], 0.0, 1.0);
    output.color = float4(colors[vertex_id], 1.0);

    return output;
}

[shader("fragment")]
float4 main_frag(VertexOutput vert) : SV_Target {
    return vert.color;
}
