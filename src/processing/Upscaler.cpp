#include "Upscaler.h"

namespace rfps {

bool Upscaler::Initialize(GPUCore& gpu, const std::wstring& shaderDir) {
    if (!gpu.CompileComputeShader(shaderDir + L"\\UpscaleSharpen.hlsl", "CSMain", m_shader)) {
        return false;
    }

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(UpscaleCB);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(gpu.GetDevice()->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer))) {
        return false;
    }

    D3D11_SAMPLER_DESC sampDesc{};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    return SUCCEEDED(gpu.GetDevice()->CreateSamplerState(&sampDesc, &m_linearSampler));
}

void Upscaler::Shutdown() {
    m_outputUAV.Reset();
    m_cachedSRV.Reset();
    m_linearSampler.Reset();
    m_constantBuffer.Reset();
    m_shader.Reset();
}

bool Upscaler::Process(
    GPUCore& gpu,
    ID3D11Texture2D* inputTex,
    UpscaleFactor factor,
    UpscaleQuality quality,
    float sharpness,
    ComPtr<ID3D11Texture2D>& outScaledTex) {

    if (!inputTex || !m_shader) return false;

    D3D11_TEXTURE2D_DESC inDesc{};
    inputTex->GetDesc(&inDesc);

    uint32_t scale = static_cast<uint32_t>(factor);
    uint32_t targetW = inDesc.Width * scale;
    uint32_t targetH = inDesc.Height * scale;

    auto* device = gpu.GetDevice();
    auto* ctx = gpu.GetContext();

    if (!outScaledTex || m_outWidth != targetW || m_outHeight != targetH) {
        outScaledTex.Reset();
        m_outputUAV.Reset();

        D3D11_TEXTURE2D_DESC outDesc{};
        outDesc.Width = targetW;
        outDesc.Height = targetH;
        outDesc.MipLevels = 1;
        outDesc.ArraySize = 1;
        outDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        outDesc.SampleDesc.Count = 1;
        outDesc.Usage = D3D11_USAGE_DEFAULT;
        outDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(device->CreateTexture2D(&outDesc, nullptr, &outScaledTex))) return false;
        if (FAILED(device->CreateUnorderedAccessView(outScaledTex.Get(), nullptr, &m_outputUAV))) return false;

        m_outWidth = targetW;
        m_outHeight = targetH;
    }

    if (m_lastInputPtr != inputTex || !m_cachedSRV) {
        m_cachedSRV.Reset();
        if (FAILED(device->CreateShaderResourceView(inputTex, nullptr, &m_cachedSRV))) return false;
        m_lastInputPtr = inputTex;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        auto* cb = reinterpret_cast<UpscaleCB*>(mapped.pData);
        cb->inputWidth = inDesc.Width;
        cb->inputHeight = inDesc.Height;
        cb->outputWidth = targetW;
        cb->outputHeight = targetH;
        cb->sharpness = sharpness;
        cb->qualityMode = static_cast<int32_t>(quality);
        cb->invInputWidth = 1.0f / static_cast<float>(inDesc.Width);
        cb->invInputHeight = 1.0f / static_cast<float>(inDesc.Height);
        ctx->Unmap(m_constantBuffer.Get(), 0);
    }

    ID3D11ShaderResourceView* srvs[] = { m_cachedSRV.Get() };
    ID3D11UnorderedAccessView* uavs[] = { m_outputUAV.Get() };
    ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
    ID3D11SamplerState* samps[] = { m_linearSampler.Get() };

    ctx->CSSetShader(m_shader.Get(), nullptr, 0);
    ctx->CSSetShaderResources(0, 1, srvs);
    ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
    ctx->CSSetConstantBuffers(0, 1, cbs);
    ctx->CSSetSamplers(0, 1, samps);

    ctx->Dispatch((targetW + 15) / 16, (targetH + 15) / 16, 1);

    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    ID3D11UnorderedAccessView* nullUAV[] = { nullptr };
    ctx->CSSetShaderResources(0, 1, nullSRV);
    ctx->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

    return true;
}

} // namespace rfps