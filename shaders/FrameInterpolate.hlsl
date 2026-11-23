Texture2D<float4> PrevFrame : register(t0);
Texture2D<float4> CurrFrame : register(t1);
Texture2D<float4> MotionVectorTex : register(t2);
RWTexture2D<float4> OutputFrame : register(u0);

SamplerState LinearSampler : register(s0);

cbuffer InterpParams : register(b0)
{
    uint2 FrameSize;
    float Alpha;
    float ArtifactSuppression;
};

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= FrameSize.x || dispatchThreadID.y >= FrameSize.y)
        return;

    float2 invSize = 1.0f / float2(FrameSize);
    float2 uv = (float2(dispatchThreadID.xy) + 0.5f) * invSize;

    float4 mvData = MotionVectorTex.Load(int3(dispatchThreadID.xy, 0));
    float2 mv = mvData.xy * invSize;
    float occlusion = mvData.z;

    float reliability = saturate(1.0f - occlusion * ArtifactSuppression);
    float2 effectiveMV = mv * reliability;

    float2 uvPrev = saturate(uv + effectiveMV * (1.0f - Alpha));
    float2 uvCurr = saturate(uv - effectiveMV * Alpha);

    float4 colorPrev = PrevFrame.SampleLevel(LinearSampler, uvPrev, 0);
    float4 colorCurr = CurrFrame.SampleLevel(LinearSampler, uvCurr, 0);

    float4 blended = lerp(colorPrev, colorCurr, Alpha);
    if (occlusion > 0.75f)
    {
        float4 nearestFrame = (Alpha < 0.5f) ? colorPrev : colorCurr;
        blended = lerp(blended, nearestFrame, saturate((occlusion - 0.75f) * 4.0f));
    }

    OutputFrame[dispatchThreadID.xy] = float4(blended.rgb, 1.0f);
}