#pragma once
#include <QAbstractNativeEventFilter>
#include <QObject>
#include <windows.h>

namespace rfps {

class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    bool RegisterDefaultHotkeys();
    void UnregisterAll();
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void toggleProcessingTriggered();
    void cycleFrameGenTriggered();
};

} // namespace rfps