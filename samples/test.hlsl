RWStructuredBuffer<uint> output : register(u0);
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) { output[id.x] = id.x * 3 + 1; }
