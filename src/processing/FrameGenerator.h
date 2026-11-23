#pragma once

#include "../core/GPUCore.h"
#include "../core/Types.h"

namespace rfps {

class FrameGenerator {
public:
    bool Initialize(GPUCore& gpu, const std::wstring& shaderDir);
    void Shutdown();

    bool ComputeMotionVectors(GPUCore& gpu, ID3D11Texture2D* prevTex, ID3D11Texture2D* currTex, int searchRadius);

    bool InterpolateFrame(
        GPUCore& gpu,
        ID3D11Texture2D* prevTex,
        ID3D11Texture2D* currTex,
        float alpha,
        float artifactSuppression,
        ComPtr<ID3D11Texture2D>& outInterpTex
    );

private:
    struct alignas(16) FlowCB {
        uint32_t width;
        uint32_t height;
        int32_t searchRadius;
        int32_t blockRadius;
    };

    struct alignas(16) InterpCB {
        uint32_t width;
        uint32_t height;
        float alpha;
        float artifactSuppression;
    };

    ComPtr<ID3D11ComputeShader> m_flowShader;
    ComPtr<ID3D11ComputeShader> m_interpShader;
    ComPtr<ID3D11Buffer> m_flowCB;
    ComPtr<ID3D11Buffer> m_interpCB;
    ComPtr<ID3D11SamplerState> m_linearSampler;

    ComPtr<ID3D11Texture2D> m_motionVectorTex;
    ComPtr<ID3D11UnorderedAccessView> m_mvUAV;
    ComPtr<ID3D11ShaderResourceView> m_mvSRV;

    ComPtr<ID3D11UnorderedAccessView> m_outUAV;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace rfps