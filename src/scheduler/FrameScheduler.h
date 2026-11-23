#pragma once

#include "../core/GPUCore.h"
#include "../capture/WindowCapture.h"
#include "../processing/Upscaler.h"
#include "../processing/FrameGenerator.h"
#include "OverlayWindow.h"
#include <atomic>
#include <thread>
#include <mutex>

namespace rfps {

class FrameScheduler {
public:
    FrameScheduler(GPUCore& gpu);
    ~FrameScheduler();

    bool Start(HWND targetHwnd, const ProcessingConfig& config, const std::wstring& shaderDir);
    void Stop();
    void UpdateConfig(const ProcessingConfig& config);

    bool IsRunning() const { return m_running.load(); }
    TelemetryStats GetStats() const;

private:
    void WorkerThread();

    GPUCore& m_gpu;
    WindowCapture m_capture;
    Upscaler m_upscaler;
    FrameGenerator m_frameGen;
    OverlayWindow m_overlay;

    std::atomic<bool> m_running{ false };
    std::thread m_thread;

    mutable std::mutex m_configMutex;
    ProcessingConfig m_config;

    mutable std::mutex m_statsMutex;
    TelemetryStats m_stats;
};

} // namespace rfps