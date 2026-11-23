#include "OverlayWindow.h"
#include <algorithm>

namespace rfps {

OverlayWindow::~OverlayWindow() {
    Destroy();
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool OverlayWindow::Create(GPUCore& gpu, HWND targetWindow) {
    Destroy();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RFPS_Overlay_Class";
    RegisterClassExW(&wc);

    RECT rect{};
    GetWindowRect(targetWindow, &rect);
    m_width = std::max<uint32_t>(1, rect.right - rect.left);
    m_height = std::max<uint32_t>(1, rect.bottom - rect.top);

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"RFPS Overlay",
        WS_POPUP,
        rect.left, rect.top, m_width, m_height,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!m_hwnd) return false;

    SetWindowDisplayAffinity(m_hwnd, WDA_EXCLUDEFROMCAPTURE);
    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);

    DXGI_SWAP_CHAIN_DESC1 scDesc{};
    scDesc.Width = m_width;
    scDesc.Height = m_height;
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    if (FAILED(gpu.GetFactory()->CreateSwapChainForHwnd(gpu.GetDevice(), m_hwnd, &scDesc, nullptr, nullptr, &m_swapChain))) {
        Destroy();
        return false;
    }

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    return true;
}

void OverlayWindow::Destroy() {
    m_swapChain.Reset();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void OverlayWindow::UpdatePosition(HWND targetWindow) {
    if (!m_hwnd || !IsWindow(targetWindow)) return;
    RECT rect{};
    if (GetWindowRect(targetWindow, &rect)) {
        uint32_t w = std::max<uint32_t>(1, rect.right - rect.left);
        uint32_t h = std::max<uint32_t>(1, rect.bottom - rect.top);
        SetWindowPos(m_hwnd, HWND_TOPMOST, rect.left, rect.top, w, h, SWP_NOACTIVATE);
    }
}

void OverlayWindow::PresentTexture(GPUCore& gpu, ID3D11Texture2D* tex, bool vsync) {
    if (!m_swapChain || !tex) return;

    ComPtr<ID3D11Texture2D> backBuffer;
    if (SUCCEEDED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        D3D11_TEXTURE2D_DESC bbDesc{}, srcDesc{};
        backBuffer->GetDesc(&bbDesc);
        tex->GetDesc(&srcDesc);

        D3D11_BOX box{ 0, 0, 0, std::min(bbDesc.Width, srcDesc.Width), std::min(bbDesc.Height, srcDesc.Height), 1 };
        gpu.GetContext()->CopySubresourceRegion(backBuffer.Get(), 0, 0, 0, 0, tex, 0, &box);
    }

    UINT syncInterval = vsync ? 1 : 0;
    UINT presentFlags = vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING;
    m_swapChain->Present(syncInterval, presentFlags);
}

} // namespace rfps