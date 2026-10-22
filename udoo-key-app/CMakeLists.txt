cmake_minimum_required(VERSION 3.20)
project(udoo-key-app VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt 6 Packages
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    SerialPort
    Concurrent
    Network
)

# Optional Qt6Svg for modern iconography
find_package(Qt6 OPTIONAL_COMPONENTS Svg SvgWidgets)

# Compiler Warnings and Strictness
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/src/acquisition
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/views
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared
)

# Core & Protocol Sources (shared between app and tests)
set(LIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/SettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/DeviceCapabilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/TaskManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ErrorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/TransactionModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/HexUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/CrcUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ShortcutManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/SessionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/CommandDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device/ITransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device/SerialTransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device/MockTransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device/DeviceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device/DeviceWorker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/StreamDemux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/BinaryLogicClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/SumpClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/TerminalClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/GpioClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/ModeClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/BusToolsClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/SyntaxClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/WirelessClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/acquisition/CaptureBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/acquisition/CaptureProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/acquisition/LodDecimator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/acquisition/TriggerExpression.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/IDecoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/DecoderRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/UartDecoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/I2cDecoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/SpiDecoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export/VcdExporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export/CsvExporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export/BinaryExporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export/InstrumentReportGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/JtagTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/JtagTap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/SwdTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/DebugProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/MockDebugTransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/SerialDebugTransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/JtagClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/SwdClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/UsbDescriptorParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/MockUsbHost.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/UsbInjectionEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/UsbTestSequence.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/UsbDeviceEmulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network/Esp32DiscoveryService.cpp
)

set(LIB_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Application.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Logger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/SettingsManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/RingBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/DeviceCapabilities.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/TaskManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ErrorManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/TransactionModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/HexUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/CrcUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ShortcutManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/SessionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/CommandDispatcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device/ITransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device/SerialTransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device/MockTransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device/DeviceManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/device/DeviceWorker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/StreamDemux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/BinaryLogicClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/SumpClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/TerminalClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/acquisition/CaptureBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/acquisition/CaptureProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/acquisition/LodDecimator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/acquisition/TriggerExpression.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/IDecoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/DecoderRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/UartDecoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/I2cDecoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/SpiDecoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export/IExporter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export/VcdExporter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export/CsvExporter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export/BinaryExporter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export/InstrumentReportGenerator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/JtagTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/JtagTap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/SwdTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/DebugProtocol.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/IDebugTransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/MockDebugTransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/SerialDebugTransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/JtagClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/SwdClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/UsbTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/UsbDescriptorParser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/IUsbHost.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/MockUsbHost.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/UsbInjectionEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/UsbTestSequence.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/UsbDeviceEmulator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network/Esp32DiscoveryService.h
)

# UI Sources
set(UI_SOURCES
    src/ui/Theme.cpp
    src/ui/MainWindow.cpp
    src/ui/widgets/HexViewWidget.cpp
    src/ui/views/LogicAnalyzerView.cpp
    src/ui/views/WaveformWidget.cpp
    src/ui/views/BusToolsView.cpp
    src/ui/views/MeterView.cpp
    src/ui/views/TerminalConsoleView.cpp
    src/ui/views/DebugView.cpp
    src/ui/views/UsbTestView.cpp
    src/ui/views/GlobalTimelineView.cpp
    src/ui/views/CommunicationDiagnosticsView.cpp
    src/ui/dialogs/ConnectionDialog.cpp
    src/ui/dialogs/ExportDialog.cpp
    src/ui/dialogs/DeviceInfoDialog.cpp
    src/ui/dialogs/CommandPaletteDialog.cpp
    src/ui/dialogs/TriggerConfigDialog.cpp
)

set(UI_HEADERS
    src/ui/Theme.h
    src/ui/MainWindow.h
    src/ui/widgets/HexViewWidget.h
    src/ui/views/LogicAnalyzerView.h
    src/ui/views/WaveformWidget.h
    src/ui/views/BusToolsView.h
    src/ui/views/MeterView.h
    src/ui/views/TerminalConsoleView.h
    src/ui/views/DebugView.h
    src/ui/views/UsbTestView.h
    src/ui/views/GlobalTimelineView.h
    src/ui/views/CommunicationDiagnosticsView.h
    src/ui/dialogs/ConnectionDialog.h
    src/ui/dialogs/ExportDialog.h
    src/ui/dialogs/DeviceInfoDialog.h
    src/ui/dialogs/CommandPaletteDialog.h
    src/ui/dialogs/TriggerConfigDialog.h
)

include(GNUInstallDirs)

# App Target
add_executable(udoo-key-app
    src/main.cpp
    resources.qrc
    ${LIB_SOURCES}
    ${LIB_HEADERS}
    ${UI_SOURCES}
    ${UI_HEADERS}
)

target_link_libraries(udoo-key-app PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Concurrent
    Qt6::Network
)

if(TARGET Qt6::Svg)
    target_link_libraries(udoo-key-app PRIVATE Qt6::Svg)
endif()

if(TARGET Qt6::SvgWidgets)
    target_link_libraries(udoo-key-app PRIVATE Qt6::SvgWidgets)
endif()

# Installation rules (FHS / Freedesktop Compliant)
install(TARGETS udoo-key-app RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX AND NOT APPLE)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../dist/linux/udoo-key.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../dist/linux/udoo-key-mime.xml
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mime/packages)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../dist/linux/icons/udoo-key.svg
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../dist/linux/99-udoo-key.rules
            DESTINATION /lib/udev/rules.d)
endif()

# CPack Packaging Configuration
set(CPACK_PACKAGE_NAME "udoo-key-workbench")
set(CPACK_PACKAGE_VENDOR "UDOO KEY Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Universal Electronics Diagnostic Instrument for UDOO KEY (RP2040+ESP32)")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_CONTACT "Andrew <support@udoo-key-instrument.org>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE")

# Debian (.deb) generator settings
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/udoo-key-project")
set(CPACK_DEBIAN_PACKAGE_SECTION "electronics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.35), libstdc++6 (>= 12), libqt6core6 (>= 6.5.0) | libqt6core6t64, libqt6gui6 (>= 6.5.0) | libqt6gui6t64, libqt6widgets6 (>= 6.5.0) | libqt6widgets6t64, libqt6serialport6 (>= 6.5.0) | libqt6serialport6t64, libqt6concurrent6 (>= 6.5.0) | libqt6concurrent6t64, libqt6svg6 (>= 6.5.0) | libqt6svg6t64")

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;TGZ")
endif()

include(CPack)

# Enable testing
enable_testing()
add_subdirectory(tests)
