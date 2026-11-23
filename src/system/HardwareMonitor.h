#pragma once
#include <windows.h>
#include <pdh.h>

namespace rfps {

class HardwareMonitor {
public:
    HardwareMonitor();
    ~HardwareMonitor();

    float GetCPUUsage();
    float GetGPUUsage();

private:
    PDH_HQUERY m_query = nullptr;
    PDH_HCOUNTER m_cpuCounter = nullptr;
    PDH_HCOUNTER m_gpuCounter = nullptr;
    bool m_initialized = false;
};

} // namespace rfps