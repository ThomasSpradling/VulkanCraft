import types;

struct GLTFMaterial {
    float4 base_color_factor;

    TextureId base_color_texture;
    SamplerId base_color_sampler;
    uint base_color_texcoord;
    float metallic_factor;

    float roughness_factor;
    TextureId metallic_roughness_texture;
    SamplerId metallic_roughness_sampler;
    uint metallic_roughness_texcoord;

    TextureId normal_texture;
    SamplerId normal_sampler;
    uint normal_texcoord;
    float normal_texture_scale;

    TextureId occlusion_texture;
    SamplerId occlusion_sampler;
    uint occlusion_texcoord;
    float occlusion_texture_strength;

    TextureId emissive_texture;
    SamplerId emissive_sampler;
    uint emissive_texcoord;
    float emissive_factor_x;

    float emissive_factor_y;
    float emissive_factor_z;
    float alpha_cutoff;
    uint double_sided;

    bool HasBaseColorTexture() {
        return base_color_texture != 0;
    }

    bool HasMetallicRoughnessTexture() {
        return metallic_roughness_texture != 0;
    }

    bool HasNormalTexture() {
        return normal_texture != 0;
    }

    bool HasOcclusionTexture() {
        return occlusion_texture != 0;
    }

    bool HasEmissiveTexture() {
        return emissive_texture != 0;
    }

    float3 GetEmissiveFactor() {
        return float3(emissive_factor_x, emissive_factor_y, emissive_factor_z);
    }

    bool IsDoubleSided() {
        return double_sided != 0;
    }
};

float3 CalculateShadingNormal(float3 geometry_normal, float4 geometry_tangent, float3 tangent_space_normal) {
    // Form tangent space basis
    float3 normal = normalize(geometry_normal);
    float3 tangent = geometry_tangent.xyz;
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
