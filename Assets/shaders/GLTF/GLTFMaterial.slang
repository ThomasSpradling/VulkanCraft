import types;
import Math;

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

float TrowbridgeReitz(float alpha, float3 shading_normal, float3 half_vector) {
    float normal_dot_half = dot(shading_normal, half_vector);
    float numerator = alpha * alpha * (normal_dot_half > 0.0f ? 1.0f : 0.0f);

    float temp = normal_dot_half * normal_dot_half * (alpha * alpha - 1) + 1;
    float denominator = PI * temp * temp;
    return numerator / denominator;
}

float SmithHelper(float alpha, float3 shading_normal, float3 shading_dir, float3 half_vector) {
    float normal_dot_light = abs(dot(shading_normal, shading_dir));

    float numerator = dot(half_vector, shading_dir) > 0.0f ? 1.0f : 0.0f;
    float denominator = normal_dot_light + sqrt(alpha * alpha + (1 - alpha * alpha) * normal_dot_light * normal_dot_light);
    return numerator / denominator;
}

float SmithMasking(float alpha, float3 shading_normal, float3 light_dir, float3 view_dir, float3 half_vector) {
    return SmithHelper(alpha, shading_normal, light_dir, half_vector) * SmithHelper(alpha, shading_normal, view_dir, half_vector);
}

float Reflectance(float f0, float3 view_dir, float3 half_vec) {
    return f0 + (1.0f - f0) * pow(1.0f - abs(dot(view_dir, half_vec)), 5);
}

float3 Reflectance(float3 f0, float3 view_dir, float3 half_vec) {
    return f0 + (1.0f - f0) * pow(1.0f - abs(dot(view_dir, half_vec)), 5);
}

float3 ConductorFresnel(float3 f0, float3 brdf, float3 view_dir, float3 half_vec) {
    return brdf * Reflectance(f0, view_dir, half_vec);
}

float3 FresnelMix(float ior, float3 base, float3 layer, float3 view_dir, float3 half_vec) {
    float f0 = ((1.0f - ior) / (1.0f + ior));
    f0 *= f0;
    float reflectance = Reflectance(f0, view_dir, half_vec);
    return lerp(base, layer, reflectance);
}

float3 DiffuseBRDF(float3 color) {
    // Lambertian BRDF

    return (1/PI) * color;
}

float3 SpecularBRDF(float alpha, float3 shading_normal, float3 light_dir, float3 view_dir) {
    float3 half_vec = normalize(light_dir + view_dir);

    return SmithMasking(alpha, shading_normal, light_dir, view_dir, half_vec) * TrowbridgeReitz(alpha, shading_normal, half_vec);
}

float3 ComputeBRDF(float3 base_color, float roughness, float metallic, float3 shading_normal, float3 light_dir, float3 view_dir) {
    if (dot(shading_normal, light_dir) <= 0.0f || dot(shading_normal, view_dir) <= 0.0f) {
        return 0.0f;
    }

    float alpha = max(roughness * roughness, 1e-4f);

    float3 diffuse_brdf = DiffuseBRDF(base_color);
    float3 specular_brdf = SpecularBRDF(alpha, shading_normal, light_dir, view_dir);

    float3 half_vec = normalize(light_dir + view_dir);

    float3 metal_brdf = ConductorFresnel(base_color, specular_brdf, view_dir, half_vec);
    float3 dielectric_brdf = FresnelMix(1.5f, diffuse_brdf, specular_brdf, view_dir, half_vec);
    return lerp(dielectric_brdf, metal_brdf, metallic);
}
