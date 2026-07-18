struct DirectionalLight {
    float4 color; // w = intensity
    float3 direction;
    float pad_;
};

struct PointLight {
    float4 color; // w = intensity
    float3 position;
    float range;
};

struct LightData {
    DirectionalLight directional_light;

    uint point_light_count;
    float pad_;
    ImmutablePtr<PointLight> point_lights;
};
