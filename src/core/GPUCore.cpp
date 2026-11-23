#include "GPUCore.h"
#include <d3dcompiler.h>
#include <algorithm>

namespace rfps {

GPUCore::~GPUCore() {
    Shutdown();
}

std::vector<GPUAdapterInfo> GPUCore::EnumerateAdapters() const {
    std::vector<GPUAdapterInfo> list;
    ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) {
        return list;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            adapter.Reset();
            continue;
        }

        GPUAdapterInfo info;
        info.index = i;
        info.description = desc.Description;
        info.dedicatedVideoMemoryMB = desc.DedicatedVideoMemory / (1024 * 1024);
        info.vendorId = desc.VendorId;
        info.isDiscrete = (desc.DedicatedVideoMemory >= 1024ULL * 1024ULL * 1024ULL);
        list.push_back(info);
        adapter.Reset();
    }
    return list;
}

bool GPUCore::Initialize(int preferredAdapterIndex) {
    Shutdown();

    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&m_factory)))) {
        return false;
    }

    auto adapters = EnumerateAdapters();
    if (adapters.empty()) {
        return false;
    }

    UINT targetIndex = 0;
    if (preferredAdapterIndex >= 0) {
        targetIndex = static_cast<UINT>(preferredAdapterIndex);
    } else {
        auto bestIt = std::max_element(adapters.begin(), adapters.end(), [](const GPUAdapterInfo& a, const GPUAdapterInfo& b) {
            if (a.isDiscrete != b.isDiscrete) return !a.isDiscrete;
            return a.dedicatedVideoMemoryMB < b.dedicatedVideoMemoryMB;
        });
        targetIndex = bestIt->index;
    }

    ComPtr<IDXGIAdapter1> adapter1;
    if (FAILED(m_factory->EnumAdapterByGpuPreference(targetIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter1)))) {
        return false;
    }
    adapter1.As(&m_adapter);

    for (const auto& info : adapters) {
        if (info.index == targetIndex) {
            m_activeAdapter = info;
            break;
        }
    }

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    HRESULT hr = D3D11CreateDevice(
        m_adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        creationFlags,
        featureLevels,
        _countof(featureLevels),
        D3D11_SDK_VERSION,
        &m_device,
        nullptr,
        &m_context
    );

    if (FAILED(hr)) {
        return false;
    }

    ComPtr<ID3D11Multithread> multithread;
    if (SUCCEEDED(m_device.As(&multithread))) {
        multithread->SetMultithreadProtected(TRUE);
    }

    return true;
}

void GPUCore::Shutdown() {
    if (m_context) {
        m_context->ClearState();
        m_context->Flush();
    }
    m_context.Reset();
    m_device.Reset();
    m_adapter.Reset();
    m_factory.Reset();
}

bool GPUCore::CompileComputeShader(const std::wstring& hlslPath, const std::string& entryPoint, ComPtr<ID3D11ComputeShader>& outShader) {
    ComPtr<ID3DBlob> csBlob;
    ComPtr<ID3DBlob> errorBlob;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    HRESULT hr = D3DCompileFromFile(
        hlslPath.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint.c_str(),
        "cs_5_0",
        flags,
        0,
        &csBlob,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = m_device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &outShader);
    return SUCCEEDED(hr);
}

void GPUCore::QueryVRAMUsage(size_t& outUsedMB, size_t& outTotalMB) const {
    outUsedMB = 0;
    outTotalMB = m_activeAdapter.dedicatedVideoMemoryMB;
    if (!m_adapter) return;

    DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
    if (SUCCEEDED(m_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
        outUsedMB = memInfo.CurrentUsage / (1024 * 1024);
    }
}

} // namespace rfps