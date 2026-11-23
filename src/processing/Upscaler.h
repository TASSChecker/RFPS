#pragma once

#include "../core/GPUCore.h"
#include "../core/Types.h"

namespace rfps {

class Upscaler {
public:
    bool Initialize(GPUCore& gpu, const std::wstring& shaderDir);
    void Shutdown();

    bool Process(
        GPUCore& gpu,
        ID3D11Texture2D* inputTex,
        UpscaleFactor factor,
        UpscaleQuality quality,
        float sharpness,
        ComPtr<ID3D11Texture2D>& outScaledTex
    );

private:
    struct alignas(16) UpscaleCB {
        uint32_t inputWidth;
        uint32_t inputHeight;
        uint32_t outputWidth;
        uint32_t outputHeight;
        float sharpness;
        int32_t qualityMode;
        float invInputWidth;
        float invInputHeight;
    };

    ComPtr<ID3D11ComputeShader> m_shader;
    ComPtr<ID3D11Buffer> m_constantBuffer;
    ComPtr<ID3D11SamplerState> m_linearSampler;

    ComPtr<ID3D11ShaderResourceView> m_cachedSRV;
    ID3D11Texture2D* m_lastInputPtr = nullptr;

    ComPtr<ID3D11UnorderedAccessView> m_outputUAV;
    uint32_t m_outWidth = 0;
    uint32_t m_outHeight = 0;
};

} // namespace rfps