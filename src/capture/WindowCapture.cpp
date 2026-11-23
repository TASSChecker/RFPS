#include "WindowCapture.h"
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <dwmapi.h>
#include <psapi.h>
#include <filesystem>

namespace rfps {

WindowCapture::~WindowCapture() {
    StopCapture();
}

std::vector<CapturableWindow> WindowCapture::EnumerateWindows() {
    std::vector<CapturableWindow> results;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (!IsWindowVisible(hwnd)) return TRUE;

        int length = GetWindowTextLengthW(hwnd);
        if (length == 0) return TRUE;

        DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
        if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

        BOOL cloaked = FALSE;
        DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
        if (cloaked) return TRUE;

        std::wstring title(length + 1, L'\0');
        GetWindowTextW(hwnd, title.data(), length + 1);
        title.resize(length);

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        std::wstring procName = L"Unknown";
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc) {
            wchar_t path[MAX_PATH]{};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, path, &size)) {
                procName = std::filesystem::path(path).filename().wstring();
            }
            CloseHandle(hProc);
        }

        auto* list = reinterpret_cast<std::vector<CapturableWindow>*>(lParam);
        list->push_back({ hwnd, title, procName });
        return TRUE;
    }, reinterpret_cast<LPARAM>(&results));

    return results;
}

bool WindowCapture::StartCapture(ID3D11Device* d3dDevice, HWND targetHwnd) {
    StopCapture();
    if (!d3dDevice || !IsWindow(targetHwnd)) return false;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(d3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) {
        return false;
    }

    winrt::com_ptr<::IInspectable> inspectableDevice;
    if (FAILED(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectableDevice.put()))) {
        return false;
    }
    m_winrtDevice = inspectableDevice.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();

    auto interopFactory = winrt::get_activation_factory<
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
        IGraphicsCaptureItemInterop>();

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };
    HRESULT hr = interopFactory->CreateForWindow(
        targetHwnd,
        winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
        winrt::put_abi(item)
    );

    if (FAILED(hr) || !item) {
        return false;
    }

    m_captureItem = item;
    m_targetHwnd = targetHwnd;
    m_lastSize = m_captureItem.Size();

    m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
        m_winrtDevice,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2,
        m_lastSize
    );

    m_frameArrivedRevoker = m_framePool.FrameArrived(winrt::auto_revoke, { this, &WindowCapture::OnFrameArrived });
    m_session = m_framePool.CreateCaptureSession(m_captureItem);
    m_session.IsBorderRequired(false);
    m_session.StartCapture();

    m_isCapturing = true;
    return true;
}

void WindowCapture::StopCapture() {
    m_isCapturing = false;
    m_frameArrivedRevoker.revoke();

    if (m_session) {
        m_session.Close();
        m_session = nullptr;
    }
    if (m_framePool) {
        m_framePool.Close();
        m_framePool = nullptr;
    }
    m_captureItem = nullptr;
    m_winrtDevice = nullptr;

    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_latestStagingTex.Reset();
    m_targetHwnd = nullptr;
}

void WindowCapture::OnFrameArrived(
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const&) {

    auto frame = sender.TryGetNextFrame();
    if (!frame) return;

    auto contentSize = frame.ContentSize();
    if (contentSize.Width != m_lastSize.Width || contentSize.Height != m_lastSize.Height) {
        m_lastSize = contentSize;
        m_framePool.Recreate(
            m_winrtDevice,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            m_lastSize
        );
    }

    auto surface = frame.Surface();
    auto access = surface.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    Microsoft::WRL::ComPtr<ID3D11Texture2D> frameTex;
    if (SUCCEEDED(access->GetInterface(IID_PPV_ARGS(&frameTex)))) {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_latestStagingTex = frameTex;
        m_frameCounter++;
    }
}

bool WindowCapture::AcquireLatestFrame(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& outTexture,
    uint64_t& outFrameId) {

    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (!m_latestStagingTex || m_frameCounter == outFrameId) {
        return false;
    }

    D3D11_TEXTURE2D_DESC srcDesc{};
    m_latestStagingTex->GetDesc(&srcDesc);

    if (!outTexture) {
        D3D11_TEXTURE2D_DESC dstDesc = srcDesc;
        dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        dstDesc.MiscFlags = 0;
        dstDesc.Usage = D3D11_USAGE_DEFAULT;
        if (FAILED(device->CreateTexture2D(&dstDesc, nullptr, &outTexture))) {
            return false;
        }
    } else {
        D3D11_TEXTURE2D_DESC dstDesc{};
        outTexture->GetDesc(&dstDesc);
        if (dstDesc.Width != srcDesc.Width || dstDesc.Height != srcDesc.Height) {
            outTexture.Reset();
            return AcquireLatestFrame(device, context, outTexture, outFrameId);
        }
    }

    context->CopyResource(outTexture.Get(), m_latestStagingTex.Get());
    outFrameId = m_frameCounter;
    return true;
}

} // namespace rfps