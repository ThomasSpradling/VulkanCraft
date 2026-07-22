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
    uint envmap_id;
}
[[vk::push_constant]] ConstantBuffer<PushConstantData> g_push_constant;

float3 ContributeDirectionalLight(DirectionalLight light, float3 surface_color, float3 shading_normal, float metallic, float roughness, float3 view_dir) {
    float power = light.color.w;
    float3 light_dir = normalize(-light.direction.xyz);
    float3 brdf = ComputeBRDF(surface_color.rgb, roughness, metallic, shading_normal, light_dir, view_dir);
    return brdf * light.color.rgb * power * max(dot(shading_normal, light_dir), 0.0f);
}

float3 ContributePointLight(PointLight light, float3 surface_color, float3 surface_position, float3 shading_normal, float metallic, float roughness, float3 view_dir) {
    float3 light_dir = light.position - surface_position;
    float distance = length(light_dir);

    if (distance < 0.0001f)
        return float3(0.0f);

    light_dir /= distance;
    
    float power = light.color.w;
    float3 brdf = ComputeBRDF(surface_color.rgb, roughness, metallic, shading_normal, light_dir, view_dir);
    return brdf * light.Attenuation(distance) * light.color.rgb * power * max(dot(shading_normal, light_dir), 0.0f);
}

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
        texture_normal.xy *= material.normal_texture_scale;
        texture_normal = normalize(texture_normal);
    
        shading_normal = CalculateShadingNormal(frag.normal, frag.tangent, texture_normal);
    }
    shading_normal = normalize(shading_normal);

    float4 surface_color = material.base_color_factor;
    if (material.HasBaseColorTexture()) {
        float2 uv = material.base_color_texcoord == 1 ? frag.uv1 : frag.uv0;
        float4 texture_color = SampleTexture2D(material.base_color_texture, material.base_color_sampler, uv);
        surface_color *= texture_color;
    }

    float roughness = material.roughness_factor;
    float metallic = material.metallic_factor;
    if (material.HasMetallicRoughnessTexture()) {
        float2 uv = material.metallic_roughness_texcoord == 1 ? frag.uv1 : frag.uv0;
        float4 texture_value = SampleTexture2D(material.metallic_roughness_texture, material.metallic_roughness_sampler, uv);
        roughness *= texture_value.g;
        metallic *= texture_value.b;
    }

    float3 emissive = material.GetEmissiveFactor();
    if (material.HasEmissiveTexture()) {
        float2 uv = material.emissive_texcoord == 1 ? frag.uv1 : frag.uv0;
        float4 texture_value = SampleTexture2D(material.emissive_texture, material.emissive_sampler, uv);
        emissive *= texture_value.rgb;
    }

    float occlusion = 1.0f;
    if (material.HasOcclusionTexture()) {
        float2 uv = material.occlusion_texcoord == 1 ? frag.uv1 : frag.uv0;
        float4 texture_value = SampleTexture2D(material.occlusion_texture, material.occlusion_sampler, uv);
        occlusion *= texture_value.r;
    }
    occlusion = 1.0f + material.occlusion_texture_strength * (occlusion - 1.0f);

    float3 view_dir = normalize(scene->eye_position.xyz - frag.world_position.xyz);

    float3 color = emissive;
    float3 ambient = surface_color.rgb * scene->ambient.rgb;
    color += ambient;
    
    color += ContributeDirectionalLight(directional_light, surface_color.rgb, shading_normal, metallic, roughness, view_dir);
    
    uint point_light_count = g_push_constant.scene_data.light_data.point_light_count;
    for (uint i = 0; i < point_light_count; ++i) {
        color += ContributePointLight(point_lights[i], surface_color.rgb, frag.world_position.xyz, shading_normal, metallic, roughness, view_dir);
    }

    float3 reflect_dir = reflect(-view_dir, shading_normal);
    float4 env_color = SampleTextureCube(g_push_constant.envmap_id, 0, reflect_dir);
    // float3 env_brdf = ComputeBRDF(surface_color.rgb, roughness, metallic, shading_normal, reflect_dir, view_dir);
    // color += env_color.rgb * env_brdf * max(dot(shading_normal, reflect_dir), 0.0f);
    
    return float4(env_color.rgb, surface_color.a);
}
