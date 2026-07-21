static constexpr float PI = radians(180);
static constexpr float TWO_PI = 2.0f * PI;
static constexpr float HALF_PI = PI / 2.0f;


// Smooth with df/dr and f both zero at r = range.
float WindowFunction(float r, float range) {
    return pow(max(1 - pow(r / range, 4), 0.0f), 2);
}

float4x4 Ortho(float left, float right, float bottom, float top) {
    float2 inv_extent = rcp(float2(right - left, top - bottom));

    float2 scale = 2.0f * inv_extent;
    float2 translate = -float2(right + left, top + bottom) * inv_extent;

    return float4x4(
        scale.x,     0.0f,        0.0f, 0.0f,
        0.0f,        scale.y,     0.0f, 0.0f,
        0.0f,        0.0f,       -1.0f, 0.0f,
        translate.x, translate.y, 0.0f, 1.0f
    );
}
