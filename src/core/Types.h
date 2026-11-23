#pragma once

#include <cstdint>
#include <string>
#include <windows.h>

namespace rfps {

enum class UpscaleFactor : int {
    Off = 1,
    X2 = 2,
    X3 = 3,
    X4 = 4
};

enum class UpscaleQuality : int {
    Performance = 0,
    Balanced = 1,
    Quality = 2
};

enum class FrameGenMode : int {
    Off = 1,
    X2 = 2,
    X3 = 3,
    X4 = 4
};

struct ProcessingConfig {
    UpscaleFactor upscaleFactor = UpscaleFactor::X2;
    UpscaleQuality upscaleQuality = UpscaleQuality::Balanced;
    float sharpness = 0.65f;

    FrameGenMode frameGenMode = FrameGenMode::X2;
    int opticalFlowRadius = 12;
    float artifactSuppression = 0.8f;

    bool vsync = true;
    int maxTargetFps = 144;
    int selectedGpuIndex = -1;
};

struct Profile {
    std::wstring name = L"Default";
    std::wstring targetProcessName;
    ProcessingConfig config;
};

struct GPUAdapterInfo {
    uint32_t index = 0;
    std::wstring description;
    size_t dedicatedVideoMemoryMB = 0;
    uint32_t vendorId = 0;
    bool isDiscrete = false;
};

struct TelemetryStats {
    double captureFps = 0.0;
    double outputFps = 0.0;
    double pipelineLatencyMs = 0.0;
    float gpuUsagePercent = 0.0f;
    float cpuUsagePercent = 0.0f;
    size_t vramUsedMB = 0;
    size_t vramTotalMB = 0;
};

} // namespace rfps