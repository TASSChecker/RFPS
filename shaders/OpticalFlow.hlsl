Texture2D<float4> PrevFrame : register(t0);
Texture2D<float4> CurrFrame : register(t1);
RWTexture2D<float4> MotionVectorTex : register(u0);

cbuffer FlowParams : register(b0)
{
    uint2 FrameSize;
    int SearchRadius;
    int BlockRadius;
};

float GetLuma(int2 coord, Texture2D<float4> tex)
{
    int2 clamped = clamp(coord, int2(0, 0), int2(FrameSize) - 1);
    float3 rgb = tex.Load(int3(clamped, 0)).rgb;
    return dot(rgb, float3(0.299, 0.587, 0.114));
}

float ComputePatchSAD(int2 centerA, int2 centerB, Texture2D<float4> texA, Texture2D<float4> texB)
{
    float sad = 0.0f;
    [unroll]
    for (int y = -2; y <= 2; y += 2)
    {
        [unroll]
        for (int x = -2; x <= 2; x += 2)
        {
            float lA = GetLuma(centerA + int2(x, y), texA);
            float lB = GetLuma(centerB + int2(x, y), texB);
            sad += abs(lA - lB);
        }
    }
    return sad;
}

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= FrameSize.x || dispatchThreadID.y >= FrameSize.y)
        return;

    int2 pos = int2(dispatchThreadID.xy);
    float bestSAD = 1e9f;
    int2 bestOffset = int2(0, 0);

    int r = SearchRadius;
    for (int dy = -r; dy <= r; dy += 2)
    {
        for (int dx = -r; dx <= r; dx += 2)
        {
            float penalty = (abs(dx) + abs(dy)) * 0.002f;
            float sad = ComputePatchSAD(pos, pos + int2(dx, dy), CurrFrame, PrevFrame) + penalty;
            if (sad < bestSAD)
            {
                bestSAD = sad;
                bestOffset = int2(dx, dy);
            }
        }
    }

    int2 coarseBest = bestOffset;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            int2 cand = coarseBest + int2(dx, dy);
            float penalty = (abs(cand.x) + abs(cand.y)) * 0.002f;
            float sad = ComputePatchSAD(pos, pos + cand, CurrFrame, PrevFrame) + penalty;
            if (sad < bestSAD)
            {
                bestSAD = sad;
                bestOffset = cand;
            }
        }
    }

    int2 prevPos = pos + bestOffset;
    float reverseSAD = ComputePatchSAD(prevPos, pos, PrevFrame, CurrFrame);
    float occlusion = saturate(abs(bestSAD - reverseSAD) * 1.5f + bestSAD * 0.25f);

    MotionVectorTex[dispatchThreadID.xy] = float4(float2(bestOffset), occlusion, 1.0f);
}