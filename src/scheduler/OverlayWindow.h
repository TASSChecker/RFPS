#pragma once

#include "../core/GPUCore.h"
#include <windows.h>

namespace rfps {

class OverlayWindow {
public:
    OverlayWindow() = default;
    ~OverlayWindow();

    bool Create(GPUCore& gpu, HWND targetWindow);
    void Destroy();

    void UpdatePosition(HWND targetWindow);
    void PresentTexture(GPUCore& gpu, ID3D11Texture2D* tex, bool vsync);
    bool IsCreated() const { return m_hwnd != nullptr; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    ComPtr<IDXGISwapChain1> m_swapChain;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace rfps