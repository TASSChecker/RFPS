#include "FrameScheduler.h"
#include <chrono>

namespace rfps {

FrameScheduler::FrameScheduler(GPUCore& gpu) : m_gpu(gpu) {}

FrameScheduler::~FrameScheduler() {
    Stop();
}

bool FrameScheduler::Start(HWND targetHwnd, const ProcessingConfig& config, const std::wstring& shaderDir) {
    Stop();
    m_config = config;

    if (!m_upscaler.Initialize(m_gpu, shaderDir)) return false;
    if (!m_frameGen.Initialize(m_gpu, shaderDir)) return false;
    if (!m_overlay.Create(m_gpu, targetHwnd)) return false;
    if (!m_capture.StartCapture(m_gpu.GetDevice(), targetHwnd)) {
        m_overlay.Destroy();
        return false;
    }

    m_running = true;
    m_thread = std::thread(&FrameScheduler::WorkerThread, this);
    return true;
}

void FrameScheduler::Stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_capture.StopCapture();
    m_overlay.Destroy();
    m_frameGen.Shutdown();
    m_upscaler.Shutdown();
}

void FrameScheduler::UpdateConfig(const ProcessingConfig& config) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
}

TelemetryStats FrameScheduler::GetStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void FrameScheduler::WorkerThread() {
    ComPtr<ID3D11Texture2D> rawFrame;
    ComPtr<ID3D11Texture2D> prevProcessedFrame;
    ComPtr<ID3D11Texture2D> interpolatedFrame;

    uint64_t lastFrameId = 0;
    uint32_t capturedCount = 0;
    uint32_t presentedCount = 0;

    auto fpsTimerStart = std::chrono::high_resolution_clock::now();

    while (m_running.load()) {
        ProcessingConfig cfg;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            cfg = m_config;
        }

        if (!IsWindow(m_capture.GetTargetWindow())) {
            m_running = false;
            break;
        }

        m_overlay.UpdatePosition(m_capture.GetTargetWindow());

        auto tStart = std::chrono::high_resolution_clock::now();
        bool hasNewFrame = m_capture.AcquireLatestFrame(m_gpu.GetDevice(), m_gpu.GetContext(), rawFrame, lastFrameId);

        if (hasNewFrame && rawFrame) {
            capturedCount++;

            ComPtr<ID3D11Texture2D> nextScaled;
            if (cfg.upscaleFactor != UpscaleFactor::Off) {
                m_upscaler.Process(m_gpu, rawFrame.Get(), cfg.upscaleFactor, cfg.upscaleQuality, cfg.sharpness, nextScaled);
            } else {
                nextScaled = rawFrame;
            }

            int multiplier = static_cast<int>(cfg.frameGenMode);
            if (multiplier > 1 && prevProcessedFrame && nextScaled) {
                m_frameGen.ComputeMotionVectors(m_gpu, prevProcessedFrame.Get(), nextScaled.Get(), cfg.opticalFlowRadius);

                for (int i = 1; i < multiplier; ++i) {
                    float alpha = static_cast<float>(i) / static_cast<float>(multiplier);
                    if (m_frameGen.InterpolateFrame(m_gpu, prevProcessedFrame.Get(), nextScaled.Get(), alpha, cfg.artifactSuppression, interpolatedFrame)) {
                        m_overlay.PresentTexture(m_gpu, interpolatedFrame.Get(), cfg.vsync);
                        presentedCount++;
                    }
                }
            }

            if (nextScaled) {
                m_overlay.PresentTexture(m_gpu, nextScaled.Get(), cfg.vsync);
                presentedCount++;

                D3D11_TEXTURE2D_DESC desc{};
                nextScaled->GetDesc(&desc);
                if (!prevProcessedFrame) {
                    m_gpu.GetDevice()->CreateTexture2D(&desc, nullptr, &prevProcessedFrame);
                }
                m_gpu.GetContext()->CopyResource(prevProcessedFrame.Get(), nextScaled.Get());
            }

            auto tEnd = std::chrono::high_resolution_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.pipelineLatencyMs = latencyMs;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }

        auto now = std::chrono::high_resolution_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - fpsTimerStart).count();
        if (elapsedSec >= 0.5) {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.captureFps = capturedCount / elapsedSec;
            m_stats.outputFps = presentedCount / elapsedSec;
            capturedCount = 0;
            presentedCount = 0;
            fpsTimerStart = now;
        }
    }
}

} // namespace rfps