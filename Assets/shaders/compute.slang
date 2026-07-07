Buffer<float> buffer0;
Buffer<float> buffer1;
RWBuffer<float> result;

uniform int count;

[shader("compute")]
[numthreads(256, 1, 1)]
void compute_main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint index = dispatch_thread_id.x;
    if (index > count)
        return;

    result[index] = buffer0[index] + buffer1[index];
}
