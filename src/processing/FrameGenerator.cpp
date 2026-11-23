#include "FrameGenerator.h"

namespace rfps {

bool FrameGenerator::Initialize(GPUCore& gpu, const std::wstring& shaderDir) {
    if (!gpu.CompileComputeShader(shaderDir + L"\\OpticalFlow.hlsl", "CSMain", m_flowShader)) return false;
    if (!gpu.CompileComputeShader(shaderDir + L"\\FrameInterpolate.hlsl", "CSMain", m_interpShader)) return false;

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = 16;
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(gpu.GetDevice()->CreateBuffer(&cbDesc, nullptr, &m_flowCB))) return false;
    if (FAILED(gpu.GetDevice()->CreateBuffer(&cbDesc, nullptr, &m_interpCB))) return false;

    D3D11_SAMPLER_DESC sampDesc{};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    return SUCCEEDED(gpu.GetDevice()->CreateSamplerState(&sampDesc, &m_linearSampler));
}

void FrameGenerator::Shutdown() {
    m_outUAV.Reset();
    m_mvSRV.Reset();
    m_mvUAV.Reset();
    m_motionVectorTex.Reset();
    m_linearSampler.Reset();
    m_interpCB.Reset();
    m_flowCB.Reset();
    m_interpShader.Reset();
    m_flowShader.Reset();
}

bool FrameGenerator::ComputeMotionVectors(GPUCore& gpu, ID3D11Texture2D* prevTex, ID3D11Texture2D* currTex, int searchRadius) {
    if (!prevTex || !currTex) return false;

    D3D11_TEXTURE2D_DESC desc{};
    currTex->GetDesc(&desc);

    auto* device = gpu.GetDevice();
    auto* ctx = gpu.GetContext();

    if (!m_motionVectorTex || m_width != desc.Width || m_height != desc.Height) {
        m_motionVectorTex.Reset();
        m_mvUAV.Reset();
        m_mvSRV.Reset();

        D3D11_TEXTURE2D_DESC mvDesc{};
        mvDesc.Width = desc.Width;
        mvDesc.Height = desc.Height;
        mvDesc.MipLevels = 1;
        mvDesc.ArraySize = 1;
        mvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        mvDesc.SampleDesc.Count = 1;
        mvDesc.Usage = D3D11_USAGE_DEFAULT;
        mvDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(device->CreateTexture2D(&mvDesc, nullptr, &m_motionVectorTex))) return false;
        if (FAILED(device->CreateUnorderedAccessView(m_motionVectorTex.Get(), nullptr, &m_mvUAV))) return false;
        if (FAILED(device->CreateShaderResourceView(m_motionVectorTex.Get(), nullptr, &m_mvSRV))) return false;

        m_width = desc.Width;
        m_height = desc.Height;
    }

    ComPtr<ID3D11ShaderResourceView> prevSRV, currSRV;
    if (FAILED(device->CreateShaderResourceView(prevTex, nullptr, &prevSRV))) return false;
    if (FAILED(device->CreateShaderResourceView(currTex, nullptr, &currSRV))) return false;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(m_flowCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        auto* cb = reinterpret_cast<FlowCB*>(mapped.pData);
        cb->width = desc.Width;
        cb->height = desc.Height;
        cb->searchRadius = searchRadius;
        cb->blockRadius = 2;
        ctx->Unmap(m_flowCB.Get(), 0);
    }

    ID3D11ShaderResourceView* srvs[] = { prevSRV.Get(), currSRV.Get() };
    ID3D11UnorderedAccessView* uavs[] = { m_mvUAV.Get() };
    ID3D11Buffer* cbs[] = { m_flowCB.Get() };

    ctx->CSSetShader(m_flowShader.Get(), nullptr, 0);
    ctx->CSSetShaderResources(0, 2, srvs);
    ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
    ctx->CSSetConstantBuffers(0, 1, cbs);

    ctx->Dispatch((desc.Width + 15) / 16, (desc.Height + 15) / 16, 1);

    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr };
    ctx->CSSetShaderResources(0, 2, nullSRVs);
    ctx->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);

    return true;
}

bool FrameGenerator::InterpolateFrame(
    GPUCore& gpu,
    ID3D11Texture2D* prevTex,
    ID3D11Texture2D* currTex,
    float alpha,
    float artifactSuppression,
    ComPtr<ID3D11Texture2D>& outInterpTex) {

    if (!prevTex || !currTex || !m_mvSRV) return false;

    auto* device = gpu.GetDevice();
    auto* ctx = gpu.GetContext();

    if (!outInterpTex) {
        m_outUAV.Reset();
        D3D11_TEXTURE2D_DESC outDesc{};
        outDesc.Width = m_width;
        outDesc.Height = m_height;
        outDesc.MipLevels = 1;
        outDesc.ArraySize = 1;
        outDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        outDesc.SampleDesc.Count = 1;
        outDesc.Usage = D3D11_USAGE_DEFAULT;
        outDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(device->CreateTexture2D(&outDesc, nullptr, &outInterpTex))) return false;
        if (FAILED(device->CreateUnorderedAccessView(outInterpTex.Get(), nullptr, &m_outUAV))) return false;
    }

    ComPtr<ID3D11ShaderResourceView> prevSRV, currSRV;
    if (FAILED(device->CreateShaderResourceView(prevTex, nullptr, &prevSRV))) return false;
    if (FAILED(device->CreateShaderResourceView(currTex, nullptr, &currSRV))) return false;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(m_interpCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        auto* cb = reinterpret_cast<InterpCB*>(mapped.pData);
        cb->width = m_width;
        cb->height = m_height;
        cb->alpha = alpha;
        cb->artifactSuppression = artifactSuppression;
        ctx->Unmap(m_interpCB.Get(), 0);
    }

    ID3D11ShaderResourceView* srvs[] = { prevSRV.Get(), currSRV.Get(), m_mvSRV.Get() };
    ID3D11UnorderedAccessView* uavs[] = { m_outUAV.Get() };
    ID3D11Buffer* cbs[] = { m_interpCB.Get() };
    ID3D11SamplerState* samps[] = { m_linearSampler.Get() };

    ctx->CSSetShader(m_interpShader.Get(), nullptr, 0);
    ctx->CSSetShaderResources(0, 3, srvs);
    ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
    ctx->CSSetConstantBuffers(0, 1, cbs);
    ctx->CSSetSamplers(0, 1, samps);

    ctx->Dispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);

    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr };
    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr };
    ctx->CSSetShaderResources(0, 3, nullSRVs);
    ctx->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);

    return true;
}

} // namespace rfps