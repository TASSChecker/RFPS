#include "HardwareMonitor.h"

namespace rfps {

HardwareMonitor::HardwareMonitor() {
    if (PdhOpenQueryW(nullptr, 0, &m_query) == ERROR_SUCCESS) {
        PdhAddEnglishCounterW(m_query, L"\\Processor(_Total)\\% Processor Time", 0, &m_cpuCounter);
        PdhAddEnglishCounterW(m_query, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &m_gpuCounter);
        PdhCollectQueryData(m_query);
        m_initialized = true;
    }
}

HardwareMonitor::~HardwareMonitor() {
    if (m_query) PdhCloseQuery(m_query);
}

float HardwareMonitor::GetCPUUsage() {
    if (!m_initialized) return 0.0f;
    PdhCollectQueryData(m_query);
    PDH_FMT_COUNTERVALUE val{};
    if (PdhGetFormattedCounterValue(m_cpuCounter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS) {
        return static_cast<float>(val.doubleValue);
    }
    return 0.0f;
}

float HardwareMonitor::GetGPUUsage() {
    if (!m_initialized) return 0.0f;
    PDH_FMT_COUNTERVALUE val{};
    if (PdhGetFormattedCounterValue(m_gpuCounter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS) {
        return static_cast<float>(val.doubleValue);
    }
    return 0.0f;
}

} // namespace rfps