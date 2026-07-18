import SceneData;
import GLTFMaterial;
import Resources;
import Lights;

import glsl;

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
    float4 world_position;

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

    output.world_position = float4(input.position, 1.0f);
    output.world_position = mul(output.world_position, g_push_constant.model);
    
    float4 position = mul(output.world_position, scene->view);
    position = mul(position, scene->projection);

    output.position = position;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    output.color = input.color;

    float3x3 model = (float3x3) g_push_constant.model;

    // Transform normal to object space
    float3x3 normal_matrix = transpose(inverse(model));
    float3 normal = normalize(mul(input.normal, normal_matrix));

    // Transform tangent to object space and remove non-orthogonal component to form orthogonal basis
    float3 tangent = normalize(mul(input.tangent.xyz, model));
    tangent = normalize(tangent - normal * dot(normal, tangent));

    output.normal = normal;
    output.tangent = float4(tangent, input.tangent.w * (determinant(model) < 0.0 ? -1.0 : 1.0));

    return output;
}

[shader("fragment")]
float4 main_frag(VertexOutput frag) : SV_Target {
    if (WIREFRAME)
        return float4(0.9f, 0.9f, 0.9f, 1.0f);
    
    ImmutablePtr<SceneData> scene = g_push_constant.scene_data;
    GLTFMaterial material = g_push_constant.materials[g_push_constant.material_id];

    DirectionalLight directional_light = scene.light_data.directional_light;
    ImmutablePtr<PointLight> point_lights = scene.light_data.point_lights;
    
    float3 shading_normal = frag.normal;
    if (material.HasNormalTexture()) {
        float2 normal_uv = material.normal_texcoord == 1 ? frag.uv1 : frag.uv0;
        
        float3 texture_normal = SampleTexture2D(material.normal_texture, material.normal_sampler, normal_uv).xyz * 2.0 - 1.0;
        texture_normal *= material.normal_texture_scale;
        texture_normal = normalize(texture_normal);
    
        shading_normal = material.normal_texture == 0 ? frag.normal : CalculateShadingNormal(frag.normal, frag.tangent, texture_normal);
    }

    float4 surface_color = frag.color * material.color_factors;
    if (material.HasAlbedoTexture()) {
        float2 albedo_uv = material.albedo_texcoord == 1 ? frag.uv1 : frag.uv0;
        float4 texture_color = SampleTexture2D(material.albedo_texture, material.albedo_sampler, albedo_uv);
        surface_color *= texture_color;
    }

    float3 color = float3(0.0f, 0.0f, 0.0f);
    float3 ambient = surface_color.rgb * scene->ambient;
    color += ambient;
    
    float sun_power = directional_light.color.w;
    float3 light_dir = normalize(-directional_light.direction.xyz);
    float3 diffuse = surface_color.rgb * directional_light.color.rgb * max(dot(light_dir, shading_normal), 0) * sun_power;
    color += diffuse;

    uint point_light_count = g_push_constant.scene_data.light_data.point_light_count;
    // if (point_light_count == 0) {
    //     return float4(float3(1.0f, 0.0f, 0.0f), frag.color.a);
    // } else {
    //     return float4(float3(0.0f, 1.0f, 0.0f), frag.color.a);
    // }

    for (uint i = 0; i < point_light_count; ++i) {
        float4 light_color = point_lights[i].color;
        float3 direction = normalize(point_lights[i].position - frag.world_position.xyz);
        color += surface_color.rgb * light_color.rgb * max(dot(direction, shading_normal), 0) * light_color.w;
    }

    return float4(color, frag.color.a);
}
