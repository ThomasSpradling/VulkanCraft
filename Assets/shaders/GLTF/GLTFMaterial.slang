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
};
