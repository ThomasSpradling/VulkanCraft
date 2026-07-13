float3x3 InverseTranspose(float3x3 m) {
    float3 r0 = m[0];
    float3 r1 = m[1];
    float3 r2 = m[2];

    float3 c0 = cross(r1, r2);
    float3 c1 = cross(r2, r0);
    float3 c2 = cross(r0, r1);

    float det = dot(r0, c0);

    return float3x3(c0, c1, c2) / det;
}
