#include "HotkeyManager.h"
#include <QCoreApplication>

namespace rfps {

constexpr int HOTKEY_TOGGLE_ID = 1001;
constexpr int HOTKEY_CYCLE_FG_ID = 1002;

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent) {
    qApp->installNativeEventFilter(this);
}

HotkeyManager::~HotkeyManager() {
    UnregisterAll();
}

bool HotkeyManager::RegisterDefaultHotkeys() {
    bool ok1 = RegisterHotKey(nullptr, HOTKEY_TOGGLE_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'S');
    bool ok2 = RegisterHotKey(nullptr, HOTKEY_CYCLE_FG_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'F');
    return ok1 && ok2;
}

void HotkeyManager::UnregisterAll() {
    UnregisterHotKey(nullptr, HOTKEY_TOGGLE_ID);
    UnregisterHotKey(nullptr, HOTKEY_CYCLE_FG_ID);
}

bool HotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, qintptr*) {
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            if (msg->wParam == HOTKEY_TOGGLE_ID) {
                emit toggleProcessingTriggered();
                return true;
            }
            if (msg->wParam == HOTKEY_CYCLE_FG_ID) {
                emit cycleFrameGenTriggered();
                return true;
            }
        }
    }
    return false;
}

} // namespace rfps