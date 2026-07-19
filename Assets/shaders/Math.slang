static constexpr float PI = radians(180);
static constexpr float TWO_PI = 2.0f * PI;
static constexpr float HALF_PI = PI / 2.0f;


// Smooth with df/dr and f both zero at r = range.
float WindowFunction(float r, float range) {
    return pow(max(1 - pow(r / range, 4), 0.0f), 2);
}
