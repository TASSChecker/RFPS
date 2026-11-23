#pragma once

#include "Types.h"
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <string>

namespace rfps {

using Microsoft::WRL::ComPtr;

class GPUCore {
public:
    GPUCore() = default;
    ~GPUCore();

    bool Initialize(int preferredAdapterIndex = -1);
    void Shutdown();

    std::vector<GPUAdapterInfo> EnumerateAdapters() const;
    const GPUAdapterInfo& GetActiveAdapterInfo() const { return m_activeAdapter; }

    ID3D11Device* GetDevice() const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }
    IDXGIFactory6* GetFactory() const { return m_factory.Get(); }

    bool CompileComputeShader(const std::wstring& hlslPath, const std::string& entryPoint, ComPtr<ID3D11ComputeShader>& outShader);
    void QueryVRAMUsage(size_t& outUsedMB, size_t& outTotalMB) const;

private:
    ComPtr<IDXGIFactory6> m_factory;
    ComPtr<IDXGIAdapter4> m_adapter;
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    GPUAdapterInfo m_activeAdapter{};
};

} // namespace rfps