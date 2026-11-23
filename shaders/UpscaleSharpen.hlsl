Texture2D<float4> InputTex : register(t0);
RWTexture2D<float4> OutputTex : register(u0);

SamplerState LinearClampSampler : register(s0);

cbuffer UpscaleParams : register(b0)
{
    uint2 InputSize;
    uint2 OutputSize;
    float Sharpness;
    int QualityMode;
    float2 InvInputSize;
};

float Luma(float3 c)
{
    return dot(c, float3(0.299, 0.587, 0.114));
}

float4 SampleCatmullRom(float2 uv)
{
    float2 samplePos = uv * InputSize;
    float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;
    float2 f = samplePos - texPos1;

    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    float2 w12 = w1 + w2;
    float2 offset12 = w2 / (w1 + w2);

    float2 texPos0 = (texPos1 - 1.0f) * InvInputSize;
    float2 texPos3 = (texPos1 + 2.0f) * InvInputSize;
    float2 texPos12 = (texPos1 + offset12) * InvInputSize;

    float4 result = 0.0f;
    result += InputTex.SampleLevel(LinearClampSampler, float2(texPos0.x, texPos0.y), 0) * w0.x * w0.y;
    result += InputTex.SampleLevel(LinearClampSampler, float2(texPos12.x, texPos0.y), 0) * w12.x * w0.y;
    result += InputTex.SampleLevel(LinearClampSampler, float2(texPos3.x, texPos0.y), 0) * w3.x * w0.y;

    result += InputTex.SampleLevel(LinearClampSampler, float2(texPos0.x, texPos12.y), 0) * w0.x * w12.y;
    result += InputTex.SampleLevel(LinearClampSampler, float2(texPos12.x, texPos12.y), 0) * w12.x * w12.y;
    result += InputTex.SampleLevel(LinearClampSampler, float2(texPos3.x, texPos12.y), 0) * w3.x * w12.y;

    result += InputTex.SampleLevel(LinearClampSampler, float2(texPos0.x, texPos3.y), 0) * w0.x * w3.y;
    result += InputTex.SampleLevel(LinearClampSampler, float2(texPos12.x, texPos3.y), 0) * w12.x * w3.y;
    result += InputTex.SampleLevel(LinearClampSampler, float2(texPos3.x, texPos3.y), 0) * w3.x * w3.y;

    return result;
}

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= OutputSize.x || dispatchThreadID.y >= OutputSize.y)
        return;

    float2 uv = (float2(dispatchThreadID.xy) + 0.5f) / float2(OutputSize);
    float4 baseColor = (QualityMode == 0)
        ? InputTex.SampleLevel(LinearClampSampler, uv, 0)
        : SampleCatmullRom(uv);

    if (Sharpness > 0.001f)
    {
        float3 n = InputTex.SampleLevel(LinearClampSampler, uv + float2(0.0f, -InvInputSize.y), 0).rgb;
        float3 s = InputTex.SampleLevel(LinearClampSampler, uv + float2(0.0f,  InvInputSize.y), 0).rgb;
        float3 e = InputTex.SampleLevel(LinearClampSampler, uv + float2( InvInputSize.x, 0.0f), 0).rgb;
        float3 w = InputTex.SampleLevel(LinearClampSampler, uv + float2(-InvInputSize.x, 0.0f), 0).rgb;

        float lC = Luma(baseColor.rgb);
        float lN = Luma(n);
        float lS = Luma(s);
        float lE = Luma(e);
        float lW = Luma(w);

        float minL = min(lC, min(min(lN, lS), min(lE, lW)));
        float maxL = max(lC, max(max(lN, lS), max(lE, lW)));

        float contrast = maxL - minL;
        float weight = saturate(1.0f - contrast * 1.5f) * Sharpness * 0.5f;

        float3 blur = (n + s + e + w) * 0.25f;
        float3 sharpened = baseColor.rgb + (baseColor.rgb - blur) * weight;

        float3 minC = min(baseColor.rgb, min(min(n, s), min(e, w)));
        float3 maxC = max(baseColor.rgb, max(max(n, s), max(e, w)));
        baseColor.rgb = clamp(sharpened, minC, maxC);
    }

    OutputTex[dispatchThreadID.xy] = float4(baseColor.rgb, 1.0f);
}