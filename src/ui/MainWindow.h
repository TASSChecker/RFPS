#pragma once

#include "../core/GPUCore.h"
#include "../scheduler/FrameScheduler.h"
#include "../system/ProfileManager.h"
#include "../system/HotkeyManager.h"
#include "../system/HardwareMonitor.h"

#include <QMainWindow>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

namespace rfps {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void RefreshWindowList();
    void OnToggleProcessing();
    void OnConfigChanged();
    void UpdateTelemetryUI();
    void OnSaveProfile();
    void OnProfileSelected(int index);

private:
    void SetupUI();
    void ApplyDarkTheme();
    ProcessingConfig GatherConfigFromUI() const;
    void ApplyConfigToUI(const ProcessingConfig& cfg);

    GPUCore m_gpu;
    FrameScheduler m_scheduler;
    ProfileManager m_profileManager;
    HotkeyManager m_hotkeyManager;
    HardwareMonitor m_hwMonitor;

    QComboBox* m_windowCombo = nullptr;
    QComboBox* m_gpuCombo = nullptr;
    QComboBox* m_profileCombo = nullptr;

    QComboBox* m_upscaleFactorCombo = nullptr;
    QComboBox* m_upscaleQualityCombo = nullptr;
    QSlider* m_sharpnessSlider = nullptr;
    QLabel* m_sharpnessValLabel = nullptr;

    QComboBox* m_frameGenCombo = nullptr;
    QSlider* m_artifactSlider = nullptr;
    QCheckBox* m_vsyncCheck = nullptr;

    QPushButton* m_startStopBtn = nullptr;

    QLabel* m_fpsBeforeLabel = nullptr;
    QLabel* m_fpsAfterLabel = nullptr;
    QLabel* m_latencyLabel = nullptr;
    QLabel* m_gpuLoadLabel = nullptr;
    QLabel* m_cpuLoadLabel = nullptr;
    QLabel* m_vramLabel = nullptr;

    QTimer* m_telemetryTimer = nullptr;
    std::vector<CapturableWindow> m_windows;
};

} // namespace rfps