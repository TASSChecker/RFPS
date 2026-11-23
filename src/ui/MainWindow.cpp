#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QCoreApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QStyle>

namespace rfps {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_scheduler(m_gpu) {
    setWindowTitle("RFPS вЂ” Real-time Frame Processing System v1.0");
    resize(820, 560);

    m_gpu.Initialize(-1);
    m_profileManager.LoadFromDisk(L"profiles.json");
    m_hotkeyManager.RegisterDefaultHotkeys();

    ApplyDarkTheme();
    SetupUI();
    RefreshWindowList();

    connect(&m_hotkeyManager, &HotkeyManager::toggleProcessingTriggered, this, &MainWindow::OnToggleProcessing);
    connect(&m_hotkeyManager, &HotkeyManager::cycleFrameGenTriggered, this, [this]() {
        int nextIdx = (m_frameGenCombo->currentIndex() + 1) % m_frameGenCombo->count();
        m_frameGenCombo->setCurrentIndex(nextIdx);
    });

    m_telemetryTimer = new QTimer(this);
    connect(m_telemetryTimer, &QTimer::timeout, this, &MainWindow::UpdateTelemetryUI);
    m_telemetryTimer->start(500);
}

MainWindow::~MainWindow() {
    m_scheduler.Stop();
    m_gpu.Shutdown();
}

void MainWindow::ApplyDarkTheme() {
    setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #0E1014;
            color: #E6E8EC;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
        }
        QGroupBox {
            background-color: #15181E;
            border: 1px solid #262B35;
            border-radius: 6px;
            margin-top: 14px;
            padding: 12px;
            font-weight: bold;
            color: #8A94A6;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
        QComboBox, QPushButton {
            background-color: #1D222B;
            border: 1px solid #303746;
            border-radius: 4px;
            padding: 6px 10px;
            color: #FFFFFF;
        }
        QComboBox:hover, QPushButton:hover {
            border-color: #3B82F6;
        }
        QPushButton#StartBtn {
            background-color: #2563EB;
            border: none;
            font-size: 14px;
            font-weight: bold;
            padding: 10px 24px;
        }
        QPushButton#StartBtn:hover {
            background-color: #1D4ED8;
        }
        QPushButton#StopBtn {
            background-color: #DC2626;
            border: none;
            font-size: 14px;
            font-weight: bold;
            padding: 10px 24px;
        }
        QLabel#MetricValue {
            font-size: 20px;
            font-weight: bold;
            color: #38BDF8;
        }
    )");
}

void MainWindow::SetupUI() {
    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(12);

    auto* topBar = new QHBoxLayout();
    m_profileCombo = new QComboBox();
    for (const auto& p : m_profileManager.GetProfiles()) {
        m_profileCombo->addItem(QString::fromStdWString(p.name));
    }
    auto* saveProfileBtn = new QPushButton("РЎРѕС…СЂР°РЅРёС‚СЊ РїСЂРѕС„РёР»СЊ");

    m_gpuCombo = new QComboBox();
    for (const auto& adapter : m_gpu.EnumerateAdapters()) {
        QString label = QString("%1 (%2 MB)").arg(QString::fromStdWString(adapter.description)).arg(adapter.dedicatedVideoMemoryMB);
        m_gpuCombo->addItem(label, adapter.index);
    }

    topBar->addWidget(new QLabel("РџСЂРѕС„РёР»СЊ:"));
    topBar->addWidget(m_profileCombo, 1);
    topBar->addWidget(saveProfileBtn);
    topBar->addSpacing(16);
    topBar->addWidget(new QLabel("Р’РёРґРµРѕРєР°СЂС‚Р°:"));
    topBar->addWidget(m_gpuCombo, 1);
    mainLayout->addLayout(topBar);

    auto* captureGroup = new QGroupBox("Р—РђРҐР’РђРў РћРљРќРђ (WINDOWS GRAPHICS CAPTURE)");
    auto* captureLayout = new QHBoxLayout(captureGroup);
    m_windowCombo = new QComboBox();
    auto* refreshWinBtn = new QPushButton("РћР±РЅРѕРІРёС‚СЊ СЃРїРёСЃРѕРє");
    captureLayout->addWidget(m_windowCombo, 1);
    captureLayout->addWidget(refreshWinBtn);
    mainLayout->addWidget(captureGroup);

    auto* settingsGrid = new QHBoxLayout();

    auto* upscaleGroup = new QGroupBox("РњРђРЎРЁРўРђР‘РР РћР’РђРќРР• (UPSCALING)");
    auto* upLayout = new QGridLayout(upscaleGroup);
    m_upscaleFactorCombo = new QComboBox();
    m_upscaleFactorCombo->addItem("Р’С‹РєР» (1x)", 1);
    m_upscaleFactorCombo->addItem("2x (RFPS Edge-Scale)", 2);
    m_upscaleFactorCombo->addItem("3x (RFPS Edge-Scale)", 3);
    m_upscaleFactorCombo->addItem("4x (RFPS Edge-Scale)", 4);
    m_upscaleFactorCombo->setCurrentIndex(1);

    m_upscaleQualityCombo = new QComboBox();
    m_upscaleQualityCombo->addItem("РџСЂРѕРёР·РІРѕРґРёС‚РµР»СЊРЅРѕСЃС‚СЊ", 0);
    m_upscaleQualityCombo->addItem("Р‘Р°Р»Р°РЅСЃ (Catmull-Rom)", 1);
    m_upscaleQualityCombo->addItem("РљР°С‡РµСЃС‚РІРѕ", 2);
    m_upscaleQualityCombo->setCurrentIndex(1);

    m_sharpnessSlider = new QSlider(Qt::Horizontal);
    m_sharpnessSlider->setRange(0, 100);
    m_sharpnessSlider->setValue(65);
    m_sharpnessValLabel = new QLabel("65%");

    upLayout->addWidget(new QLabel("РљРѕСЌС„С„РёС†РёРµРЅС‚:"), 0, 0);
    upLayout->addWidget(m_upscaleFactorCombo, 0, 1, 1, 2);
    upLayout->addWidget(new QLabel("Р РµР¶РёРј РєР°С‡РµСЃС‚РІР°:"), 1, 0);
    upLayout->addWidget(m_upscaleQualityCombo, 1, 1, 1, 2);
    upLayout->addWidget(new QLabel("Р РµР·РєРѕСЃС‚СЊ:"), 2, 0);
    upLayout->addWidget(m_sharpnessSlider, 2, 1);
    upLayout->addWidget(m_sharpnessValLabel, 2, 2);

    auto* fgGroup = new QGroupBox("Р“Р•РќР•Р РђР¦РРЇ РљРђР”Р РћР’ (OPTICAL FLOW FG)");
    auto* fgLayout = new QGridLayout(fgGroup);
    m_frameGenCombo = new QComboBox();
    m_frameGenCombo->addItem("Р’С‹РєР» (1x)", 1);
    m_frameGenCombo->addItem("x2 (1 РїСЂРѕРјРµР¶СѓС‚РѕС‡РЅС‹Р№ РєР°РґСЂ)", 2);
    m_frameGenCombo->addItem("x3 (2 РїСЂРѕРјРµР¶СѓС‚РѕС‡РЅС‹С… РєР°РґСЂР°)", 3);
    m_frameGenCombo->addItem("x4 (3 РїСЂРѕРјРµР¶СѓС‚РѕС‡РЅС‹С… РєР°РґСЂР°)", 4);
    m_frameGenCombo->setCurrentIndex(1);

    m_artifactSlider = new QSlider(Qt::Horizontal);
    m_artifactSlider->setRange(0, 100);
    m_artifactSlider->setValue(80);

    m_vsyncCheck = new QCheckBox("Р’РµСЂС‚РёРєР°Р»СЊРЅР°СЏ СЃРёРЅС…СЂРѕРЅРёР·Р°С†РёСЏ (VSync)");
    m_vsyncCheck->setChecked(true);

    fgLayout->addWidget(new QLabel("РњРЅРѕР¶РёС‚РµР»СЊ FPS:"), 0, 0);
    fgLayout->addWidget(m_frameGenCombo, 0, 1);
    fgLayout->addWidget(new QLabel("РџРѕРґР°РІР»РµРЅРёРµ Р°СЂС‚РµС„Р°РєС‚РѕРІ:"), 1, 0);
    fgLayout->addWidget(m_artifactSlider, 1, 1);
    fgLayout->addWidget(m_vsyncCheck, 2, 0, 1, 2);

    settingsGrid->addWidget(upscaleGroup);
    settingsGrid->addWidget(fgGroup);
    mainLayout->addLayout(settingsGrid);

    auto* telemetryGroup = new QGroupBox("РњРћРќРРўРћР РРќР“ РџР РћРР—Р’РћР”РРўР•Р›Р¬РќРћРЎРўР Р’ Р Р•РђР›Р¬РќРћРњ Р’Р Р•РњР•РќР");
    auto* teleGrid = new QGridLayout(telemetryGroup);

    m_fpsBeforeLabel = new QLabel("0.0 FPS");
    m_fpsBeforeLabel->setObjectName("MetricValue");
    m_fpsAfterLabel = new QLabel("0.0 FPS");
    m_fpsAfterLabel->setObjectName("MetricValue");
    m_latencyLabel = new QLabel("0.0 ms");
    m_gpuLoadLabel = new QLabel("0%");
    m_cpuLoadLabel = new QLabel("0%");
    m_vramLabel = new QLabel("0 / 0 MB");

    teleGrid->addWidget(new QLabel("FPS РґРѕ РѕР±СЂР°Р±РѕС‚РєРё:"), 0, 0);
    teleGrid->addWidget(m_fpsBeforeLabel, 1, 0);
    teleGrid->addWidget(new QLabel("FPS РїРѕСЃР»Рµ РѕР±СЂР°Р±РѕС‚РєРё:"), 0, 1);
    teleGrid->addWidget(m_fpsAfterLabel, 1, 1);
    teleGrid->addWidget(new QLabel("Р—Р°РґРµСЂР¶РєР° РєРѕРЅРІРµР№РµСЂР°:"), 0, 2);
    teleGrid->addWidget(m_latencyLabel, 1, 2);

    teleGrid->addWidget(new QLabel("РќР°РіСЂСѓР·РєР° GPU:"), 2, 0);
    teleGrid->addWidget(m_gpuLoadLabel, 3, 0);
    teleGrid->addWidget(new QLabel("РќР°РіСЂСѓР·РєР° CPU:"), 2, 1);
    teleGrid->addWidget(m_cpuLoadLabel, 3, 1);
    teleGrid->addWidget(new QLabel("Р’РёРґРµРѕРїР°РјСЏС‚СЊ (VRAM):"), 2, 2);
    teleGrid->addWidget(m_vramLabel, 3, 2);

    mainLayout->addWidget(telemetryGroup);

    auto* bottomBar = new QHBoxLayout();
    auto* hotkeyHint = new QLabel("Р“РѕСЂСЏС‡РёРµ РєР»Р°РІРёС€Рё: Ctrl+Alt+S (РЎС‚Р°СЂС‚/РЎС‚РѕРї) | Ctrl+Alt+F (Р РµР¶РёРј FG)");
    hotkeyHint->setStyleSheet("color: #6B7280;");
    m_startStopBtn = new QPushButton("Р—РђРџРЈРЎРўРРўР¬ РћР‘Р РђР‘РћРўРљРЈ");
    m_startStopBtn->setObjectName("StartBtn");

    bottomBar->addWidget(hotkeyHint);
    bottomBar->addStretch();
    bottomBar->addWidget(m_startStopBtn);
    mainLayout->addLayout(bottomBar);

    setCentralWidget(central);

    connect(refreshWinBtn, &QPushButton::clicked, this, &MainWindow::RefreshWindowList);
    connect(m_startStopBtn, &QPushButton::clicked, this, &MainWindow::OnToggleProcessing);
    connect(saveProfileBtn, &QPushButton::clicked, this, &MainWindow::OnSaveProfile);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::OnProfileSelected);

    connect(m_sharpnessSlider, &QSlider::valueChanged, this, [this](int val) {
        m_sharpnessValLabel->setText(QString("%1%").arg(val));
        OnConfigChanged();
    });
    connect(m_upscaleFactorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::OnConfigChanged);
    connect(m_upscaleQualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::OnConfigChanged);
    connect(m_frameGenCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::OnConfigChanged);
    connect(m_artifactSlider, &QSlider::valueChanged, this, &MainWindow::OnConfigChanged);
    connect(m_vsyncCheck, &QCheckBox::toggled, this, &MainWindow::OnConfigChanged);
}

void MainWindow::RefreshWindowList() {
    m_windowCombo->clear();
    m_windows = WindowCapture::EnumerateWindows();
    HWND selfHwnd = reinterpret_cast<HWND>(winId());

    for (const auto& w : m_windows) {
        if (w.hwnd == selfHwnd) continue;
        QString label = QString("[%1] %2")
            .arg(QString::fromStdWString(w.processName))
            .arg(QString::fromStdWString(w.title));
        m_windowCombo->addItem(label, reinterpret_cast<qulonglong>(w.hwnd));
    }
}

ProcessingConfig MainWindow::GatherConfigFromUI() const {
    ProcessingConfig cfg;
    cfg.upscaleFactor = static_cast<UpscaleFactor>(m_upscaleFactorCombo->currentData().toInt());
    cfg.upscaleQuality = static_cast<UpscaleQuality>(m_upscaleQualityCombo->currentData().toInt());
    cfg.sharpness = m_sharpnessSlider->value() / 100.0f;
    cfg.frameGenMode = static_cast<FrameGenMode>(m_frameGenCombo->currentData().toInt());
    cfg.artifactSuppression = m_artifactSlider->value() / 100.0f;
    cfg.vsync = m_vsyncCheck->isChecked();
    cfg.selectedGpuIndex = m_gpuCombo->currentData().toInt();
    return cfg;
}

void MainWindow::ApplyConfigToUI(const ProcessingConfig& cfg) {
    m_upscaleFactorCombo->setCurrentIndex(m_upscaleFactorCombo->findData(static_cast<int>(cfg.upscaleFactor)));
    m_upscaleQualityCombo->setCurrentIndex(m_upscaleQualityCombo->findData(static_cast<int>(cfg.upscaleQuality)));
    m_sharpnessSlider->setValue(static_cast<int>(cfg.sharpness * 100));
    m_frameGenCombo->setCurrentIndex(m_frameGenCombo->findData(static_cast<int>(cfg.frameGenMode)));
    m_artifactSlider->setValue(static_cast<int>(cfg.artifactSuppression * 100));
    m_vsyncCheck->setChecked(cfg.vsync);
}

void MainWindow::OnConfigChanged() {
    if (m_scheduler.IsRunning()) {
        m_scheduler.UpdateConfig(GatherConfigFromUI());
    }
}

void MainWindow::OnToggleProcessing() {
    if (m_scheduler.IsRunning()) {
        m_scheduler.Stop();
        m_startStopBtn->setText("Р—РђРџРЈРЎРўРРўР¬ РћР‘Р РђР‘РћРўРљРЈ");
        m_startStopBtn->setObjectName("StartBtn");
        m_startStopBtn->style()->polish(m_startStopBtn);
        return;
    }

    if (m_windowCombo->count() == 0) {
        QMessageBox::warning(this, "RFPS", "РќРµ РІС‹Р±СЂР°РЅРѕ С†РµР»РµРІРѕРµ РѕРєРЅРѕ РґР»СЏ Р·Р°С…РІР°С‚Р°.");
        return;
    }

    HWND targetHwnd = reinterpret_cast<HWND>(m_windowCombo->currentData().toULongLong());
    ProcessingConfig cfg = GatherConfigFromUI();

    if (cfg.selectedGpuIndex != static_cast<int>(m_gpu.GetActiveAdapterInfo().index)) {
        m_gpu.Initialize(cfg.selectedGpuIndex);
    }

    std::wstring shaderDir = (QCoreApplication::applicationDirPath() + "/shaders").toStdWString();
    if (m_scheduler.Start(targetHwnd, cfg, shaderDir)) {
        m_startStopBtn->setText("РћРЎРўРђРќРћР’РРўР¬ РћР‘Р РђР‘РћРўРљРЈ");
        m_startStopBtn->setObjectName("StopBtn");
        m_startStopBtn->style()->polish(m_startStopBtn);
    } else {
        QMessageBox::critical(this, "РћС€РёР±РєР° Р·Р°РїСѓСЃРєР°", "РќРµ СѓРґР°Р»РѕСЃСЊ РёРЅРёС†РёР°Р»РёР·РёСЂРѕРІР°С‚СЊ Р·Р°С…РІР°С‚ РѕРєРЅР° РёР»Рё РєРѕРјРїРёР»СЏС†РёСЋ HLSL С€РµР№РґРµСЂРѕРІ.");
    }
}

void MainWindow::UpdateTelemetryUI() {
    TelemetryStats stats = m_scheduler.GetStats();
    size_t vramUsed = 0, vramTotal = 0;
    m_gpu.QueryVRAMUsage(vramUsed, vramTotal);

    m_fpsBeforeLabel->setText(QString("%1 FPS").arg(stats.captureFps, 0, 'f', 1));
    m_fpsAfterLabel->setText(QString("%1 FPS").arg(stats.outputFps, 0, 'f', 1));
    m_latencyLabel->setText(QString("%1 ms").arg(stats.pipelineLatencyMs, 0, 'f', 2));
    m_gpuLoadLabel->setText(QString("%1%").arg(m_hwMonitor.GetGPUUsage(), 0, 'f', 0));
    m_cpuLoadLabel->setText(QString("%1%").arg(m_hwMonitor.GetCPUUsage(), 0, 'f', 0));
    m_vramLabel->setText(QString("%1 / %2 MB").arg(vramUsed).arg(vramTotal));
}

void MainWindow::OnSaveProfile() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "РЎРѕС…СЂР°РЅРёС‚СЊ РїСЂРѕС„РёР»СЊ", "РќР°Р·РІР°РЅРёРµ РїСЂРѕС„РёР»СЏ:", QLineEdit::Normal, "Game Profile", &ok);
    if (!ok || name.isEmpty()) return;

    Profile p;
    p.name = name.toStdWString();
    p.config = GatherConfigFromUI();
    m_profileManager.AddOrUpdateProfile(p);
    m_profileManager.SaveToDisk(L"profiles.json");

    if (m_profileCombo->findText(name) == -1) {
        m_profileCombo->addItem(name);
    }
    m_profileCombo->setCurrentText(name);
}

void MainWindow::OnProfileSelected(int index) {
    auto& profiles = m_profileManager.GetProfiles();
    if (index >= 0 && index < static_cast<int>(profiles.size())) {
        ApplyConfigToUI(profiles[index].config);
    }
}

} // namespace rfps