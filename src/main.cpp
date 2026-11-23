#include "ui/MainWindow.h"
#include <QApplication>
#include <winrt/base.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    int argc = 0;
    QApplication app(argc, nullptr);

    rfps::MainWindow window;
    window.show();

    int result = app.exec();
    winrt::uninit_apartment();
    return result;
}