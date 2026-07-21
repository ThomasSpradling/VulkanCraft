[[vk::binding(0, 0)]]
Texture2D<float4> g_textures[];

[[vk::binding(1, 0)]]
TextureCube<float4> g_cube_textures[];

[[vk::binding(2, 0)]]
SamplerState g_samplers[];

[[vk::binding(3, 0)]]
RWTexture2D<float4> g_storage_images[];

[[vk::binding(4, 0)]]
RaytracingAccelerationStructure g_acceleration_structures[];

Texture2D<float4> GetTexture(uint id) {
    return g_textures[NonUniformResourceIndex(id)];
}

TextureCube<float4> GetTextureCube(uint id) {
    return g_cube_textures[NonUniformResourceIndex(id)];
}

SamplerState GetSampler(uint id) {
    return g_samplers[NonUniformResourceIndex(id)];
}

float4 SampleTexture2D(uint texture_id, uint sampler_id, float2 uv) {
    return GetTexture(texture_id).Sample(GetSampler(sampler_id), uv);
}

float4 SampleTextureCube(uint texture_id, uint sampler_id, float3 location) {
    return GetTextureCube(texture_id).Sample(GetSampler(sampler_id), location);
}

RWTexture2D<float4> GetStorageImage(uint id) {
    return g_storage_images[NonUniformResourceIndex(id)];
}

RaytracingAccelerationStructure GetAccelerationStructure(uint id) {
    return g_acceleration_structures[NonUniformResourceIndex(id)];
}
