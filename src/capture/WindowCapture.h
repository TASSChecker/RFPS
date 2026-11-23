#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <windows.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <mutex>
#include <vector>
#include <string>

namespace rfps {

struct CapturableWindow {
    HWND hwnd = nullptr;
    std::wstring title;
    std::wstring processName;
};

class WindowCapture {
public:
    WindowCapture() = default;
    ~WindowCapture();

    static std::vector<CapturableWindow> EnumerateWindows();

    bool StartCapture(ID3D11Device* d3dDevice, HWND targetHwnd);
    void StopCapture();

    bool AcquireLatestFrame(ID3D11Device* device, ID3D11DeviceContext* context, Microsoft::WRL::ComPtr<ID3D11Texture2D>& outTexture, uint64_t& outFrameId);
    bool IsCapturing() const { return m_isCapturing; }
    HWND GetTargetWindow() const { return m_targetHwnd; }

private:
    void OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

    HWND m_targetHwnd = nullptr;
    bool m_isCapturing = false;

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_winrtDevice{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker m_frameArrivedRevoker;

    std::mutex m_frameMutex;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_latestStagingTex;
    uint64_t m_frameCounter = 0;
    winrt::Windows::Graphics::SizeInt32 m_lastSize{};
};

} // namespace rfps