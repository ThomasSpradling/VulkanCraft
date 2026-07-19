import Math;

struct DirectionalLight {
    float4 color; // w = intensity
    float3 direction;
    float pad_;
};

struct PointLight {
    float4 color; // w = intensity
    float3 position;
    float range;

    float Attenuation(float r) {
        float attenuation = 1.0f / (r * r + 1.0f);
        if (range <= 0.0f) {
            return attenuation;
        }

        return WindowFunction(r, range) * attenuation;
    }
};

struct LightData {
    DirectionalLight directional_light;

    uint point_light_count;
    float pad_;
    ImmutablePtr<PointLight> point_lights;
};
