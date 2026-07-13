import types;

struct GLTFMaterial {
    float4 color_factors;

    uint32_t albedo_texcoord;
    uint32_t normal_texcoord;

    TextureId albedo_texture;
    SamplerId albedo_sampler;

    TextureId normal_texture;
    SamplerId normal_sampler;
};
