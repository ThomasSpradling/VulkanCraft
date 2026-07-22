import Lights;

struct SceneData {
    float4x4 projection;
    float4x4 view;
    float4 sun_direction; // w = power
    float4 eye_position;
    float4 ambient;
    ImmutablePtr<LightData> light_data;
};
