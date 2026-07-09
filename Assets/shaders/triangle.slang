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

struct GlobalSceneData {
    float4x4 projection;
    float4x4 view;
    float4 sun_direction; // w = power
    float ambient;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalSceneData> g_scene;

struct PushConstantData {
    float4x4 model;
}

[[vk::push_constant]]
ConstantBuffer<PushConstantData> g_push_constant;

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

[shader("vertex")]
VertexOutput main_vert(VertexInput input) {
    VertexOutput output;

    float4 position = float4(input.position, 1.0);
    position = mul(position, g_push_constant.model);
    position = mul(position, g_scene.view);
    position = mul(position, g_scene.projection);

    output.position = position;
    output.color = input.color;
    output.normal = normalize(mul(float4(input.normal, 0.0), g_push_constant.model).xyz);;
    output.uv = float2(input.uv_x, input.uv_y);

    return output;
}

[shader("fragment")]
float4 main_frag(VertexOutput frag) : SV_Target {
    float power = g_scene.sun_direction.w;
    float3 normal = normalize(frag.normal);
    float3 light_dir = normalize(-g_scene.sun_direction.xyz);

    float3 ambient = frag.color.rgb * g_scene.ambient;
    float3 diffuse = frag.color.rgb * max(dot(light_dir, normal), 0) * g_scene.sun_direction.w;
    return float4(ambient + diffuse, frag.color.a);
}
