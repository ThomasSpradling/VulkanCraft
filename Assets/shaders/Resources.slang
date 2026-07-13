[[vk::binding(0, 0)]]
Texture2D<float4> g_textures[];

[[vk::binding(1, 0)]]
SamplerState g_samplers[];

[[vk::binding(2, 0)]]
RWTexture2D<float4> g_storage_images[];

[[vk::binding(3, 0)]]
RaytracingAccelerationStructure g_acceleration_structures[];

Texture2D<float4> GetTexture(uint id) {
    return g_textures[id];
}

SamplerState GetSampler(uint id) {
    return g_samplers[id];
}

float4 SampleTexture2D(uint texture_id, uint sampler_id, float2 uv) {
    return g_textures[texture_id].Sample(g_samplers[sampler_id], uv);
}

RWTexture2D<float4> GetStorageImage(uint id) {
    return g_storage_images[id];
}

RaytracingAccelerationStructure GetAccelerationStructure(uint id) {
    return g_acceleration_structures[id];
}
