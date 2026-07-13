import SceneData;
import GLTFMaterial;
import Resources;
import Math;

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
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    output.color = input.color;

    float3x3 model = (float3x3) g_push_constant.model;

    // Transform normal to object space
    float3x3 normal_matrix = InverseTranspose(model);
    float3 normal = normalize(mul(input.normal, normal_matrix));

    // Transform tangent to object space and remove non-orthogonal component to form orthogonal basis
    float3 tangent = normalize(mul(input.tangent.xyz, model));
    tangent = normalize(tangent - normal * dot(normal, tangent));

    output.normal = normal;
    output.tangent = float4(tangent, input.tangent.w * (determinant(model) < 0.0 ? -1.0 : 1.0));

    return output;
}

float3 CalculateShadingNormal(float3 geometry_normal, float4 geometry_tangent, float3 tangent_space_normal) {
    // Form shading frame tangent space basis
    float3 normal = normalize(geometry_normal);
    float3 tangent = geometry_normal.xyz;
    tangent = normalize(tangent - normal * dot(normal, tangent));
    float3 bitangent = geometry_tangent.w * cross(normal, tangent);

    // Convert from tangent space to world space basis
    float3 shading_normal = normalize(
        tangent * tangent_space_normal.x +
        bitangent * tangent_space_normal.y +
        normal * tangent_space_normal.z
    );
    return shading_normal;
}

[shader("fragment")]
float4 main_frag(VertexOutput frag) : SV_Target {
    if (WIREFRAME)
        return float4(0.9f, 0.9f, 0.9f, 1.0f);
    
    ImmutablePtr<SceneData> scene = g_push_constant.scene_data;
    GLTFMaterial material = g_push_constant.materials[g_push_constant.material_id];

    float2 normal_uv = material.normal_texcoord == 1 ? frag.uv1 : frag.uv0;

    float3 texture_normal = SampleTexture2D(material.normal_texture, material.normal_sampler, normal_uv).xyz * 2.0 - 1.0;
    texture_normal *= material.normal_texture_scale;
    texture_normal = normalize(texture_normal);

    float3 shading_normal = CalculateShadingNormal(frag.normal, frag.tangent, texture_normal);

    // float3 color = frag.color.rgb;
    // float3 texture_color = SampleTexture2D(material.albedo_texture, material.albedo_sampler, frag.uv0).rgb;
    // float3 color = 0.5f * texture_color + 0.5f * frag.color.rgb;
    // float3 color = texture_color;

    // float power = scene->sun_direction.w;
    // float3 normal = normalize(frag.normal);
    // float3 light_dir = normalize(-scene->sun_direction.xyz);

    // float3 ambient = color * scene->ambient;
    // float3 diffuse = color * max(dot(light_dir, normal), 0) * scene->sun_direction.w;
    // return float4(ambient + diffuse, frag.color.a);
    // return float4(color, 1.0f);
    
    return float4(frag.normal, 1.0f);

}
