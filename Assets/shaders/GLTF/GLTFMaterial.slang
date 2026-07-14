import types;

struct GLTFMaterial {
    float4 color_factors;

    TextureId albedo_texture;
    SamplerId albedo_sampler;
    uint32_t albedo_texcoord;
    
    // Normal texture
    TextureId normal_texture;
    SamplerId normal_sampler;
    uint32_t normal_texcoord;
    float normal_texture_scale;

    bool HasAlbedoTexture() {
        return albedo_texture > 0;
    }

    bool HasNormalTexture() {
        return normal_texture > 0;
    }
};

float3 CalculateShadingNormal(float3 geometry_normal, float4 geometry_tangent, float3 tangent_space_normal) {
    // Form tangent space basis
    float3 normal = normalize(geometry_normal);
    float3 tangent = geometry_normal.xyz;
    tangent = normalize(tangent - normal * dot(normal, tangent));
    float3 bitangent = geometry_tangent.w * cross(normal, tangent);

    // Change of basis back to world space
    float3 shading_normal = normalize(
        tangent * tangent_space_normal.x +
        bitangent * tangent_space_normal.y +
        normal * tangent_space_normal.z
    );
    return shading_normal;
}
