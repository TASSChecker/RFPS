cmake_minimum_required(VERSION 3.24)
project(RFPS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT WIN32)
    message(FATAL_ERROR "RFPS supports Windows 11 x64 only.")
endif()

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    src/main.cpp
    src/core/GPUCore.cpp
    src/capture/WindowCapture.cpp
    src/processing/Upscaler.cpp
    src/processing/FrameGenerator.cpp
    src/scheduler/FrameScheduler.cpp
    src/scheduler/OverlayWindow.cpp
    src/system/ProfileManager.cpp
    src/system/HotkeyManager.cpp
    src/system/HardwareMonitor.cpp
    src/ui/MainWindow.cpp
)

set(HEADERS
    src/core/Types.h
    src/core/GPUCore.h
    src/capture/WindowCapture.h
    src/processing/Upscaler.h
    src/processing/FrameGenerator.h
    src/scheduler/FrameScheduler.h
    src/scheduler/OverlayWindow.h
    src/system/ProfileManager.h
    src/system/HotkeyManager.h
    src/system/HardwareMonitor.h
    src/ui/MainWindow.h
)

add_executable(RFPS WIN32 ${SOURCES} ${HEADERS})

target_include_directories(RFPS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(RFPS PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
    dwmapi.lib
    windowsapp.lib
    pdh.lib
)

add_custom_command(TARGET RFPS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:RFPS>/shaders
)