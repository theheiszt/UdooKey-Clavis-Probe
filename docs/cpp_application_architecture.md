# UDOO KEY — Native Cross-Platform C++ Application Architecture

**Document ID:** DOC-CPP-ARCH-20261003  
**Status:** SPECIFICATION & DESIGN (PHASE 1)  
**Date:** 2026-10-03  
**Target Application:** Native Desktop Companion (`udoo-key-app`)  
**Target Platforms:** Windows 10/11 (MSVC / Clang) & Linux (GCC / Clang)  
**Technology Stack:** Modern C++ (C++17 / C++20), Qt 6.5+, CMake 3.22+  

---

## 1. Architectural Philosophy & Layered Hierarchy

The native desktop application functions as the professional PC-side companion interface for the UDOO KEY hardware platform. To ensure responsiveness, long-term maintainability, platform independence, and architectural integrity, the application is organized into strictly separated layers:

```text
                               UDOO KEY HARDWARE
                                       │
                             USB CDC / Serial Link
                                       │
                                       ▼
                    ┌─────────────────────────────────────┐
                    │          C++ Transport Layer        │
                    │  (SerialTransport, MockTransport)   │
                    └──────────────────┬──────────────────┘
                                       │
                                       ▼
                    ┌─────────────────────────────────────┐
                    │          C++ Protocol Layer         │
                    │  (SumpClient, BinaryProtocol, CLI)  │
                    └──────────────────┬──────────────────┘
                                       │
                                       ▼
                    ┌─────────────────────────────────────┐
                    │           C++ Device Layer          │
                    │  (UdooDevice, CapabilityManager)    │
                    └──────────────────┬──────────────────┘
                                       │
        ┌──────────────────────────────┼──────────────────────────────┐
        │                              │                              │
        ▼                              ▼                              ▼
┌───────────────┐              ┌───────────────┐              ┌───────────────┐
│ Logic Engine  │              │  Meter Engine │              │Bus Tool Engine│
│(Capture/Buffer│              │(ADC, Calibr., │              │(I2C, SPI, UART│
│ /Decimation)  │              │ Continuity)   │              │ 1-Wire, USB)  │
└───────┬───────┘              └───────┬───────┘              └───────┬───────┘
        │                              │                              │
        └──────────────────────────────┼──────────────────────────────┘
                                       │
                                       ▼
                    ┌─────────────────────────────────────┐
                    │          Application Core           │
                    │ (SessionManager, EventBus, Settings)│
                    └──────────────────┬──────────────────┘
                                       │
                 ┌─────────────────────┴─────────────────────┐
                 │                                           │
                 ▼                                           ▼
  ┌─────────────────────────────┐             ┌─────────────────────────────┐
  │       GUI / UI Views        │             │        Data & Export        │
  │ (WaveformViewer, TermView,  │             │  (VCD, CSV, JSON Session,   │
  │  MeterView, BusToolViews)   │             │   Image Snapshots, Logs)    │
  └─────────────────────────────┘             └─────────────────────────────┘
```

### Invariant Rules:
1. **Zero Hardware Leaks into UI**: GUI components (`QWidget`, `QMainWindow`) never interact with serial ports, raw byte buffers, framing bytes, or SUMP registers.
2. **Asynchronous Execution**: Long-running device I/O, capture downloading, decimation calculations, protocol decoding, and disk exports are strictly barred from the main GUI thread.
3. **Pluggable Transports**: The device session interacts with an abstract `ITransport` interface, allowing full automated execution against a physical COM port or a headless `MockTransport` simulator.
4. **Single Cross-Platform Codebase**: All GUI and system logic uses Qt 6 and standard C++ library primitives; no Win32-specific (`windows.h`) or POSIX-specific APIs are permitted in application logic.

---

## 2. Multi-Threaded Execution Model

To eliminate the UI stutter, dropped frames, and frozen cursors exhibited by the Python prototype, the C++ application employs an actor/worker threading architecture leveraging Qt 6's event loop and signal/slot dispatching:

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                              GUI THREAD                                │
 │  • Qt Main Event Loop & Windowing                                      │
 │  • User Interaction (Mouse, Keyboard, Cursors, Zoom/Pan)               │
 │  • Native Waveform Painting & Canvas Updates                           │
 │  • Model/View Rendering (Tables, Gauges, Status Indicators)            │
 └───────▲──────────────────────▲─────────────────────▲────────────────▲──┘
         │                      │                     │                │
         │ Signals / Slots      │ Signals / Slots     │ Signals/Slots  │ Signals/Slots
         ▼                      ▼                     ▼                ▼
 ┌───────────────┐      ┌───────────────┐     ┌───────────────┐ ┌───────────────┐
 │ Device Worker │      │    Capture    │     │Decoder Worker │ │ Export Worker │
 │    Thread     │      │   Processor   │     │    Thread     │ │    Thread     │
 ├───────────────┤      ├───────────────┤     ├───────────────┤ ├───────────────┤
 │• QSerialPort  │      │• Ring-Buffer  │     │• Background   │ │• Async VCD    │
 │  Event Loop   │      │  Alignment    │     │  UART Parsing │ │  Generation   │
 │• Non-Blocking │      │• Level-of-    │     │• I2C Transact.│ │• Tabular CSV  │
 │  Deframing    │      │  Detail (LOD) │     │  Extraction   │ │  Streaming    │
 │• Binary & SUMP│      │  Min/Max      │     │• SPI Frame    │ │• High-Volume  │
 │  Parsing      │      │  Decimation   │     │  Assembly     │ │  Disk I/O     │
 │• Auto-Recovery│      │• Peak Detect  │     │• USB Sniffer  │ │• Thread-Safe  │
 │  & Heartbeat  │      │• Fast Bitwise │     │  Decodes      │ │  File Locking │
 └───────────────┘      └───────────────┘     └───────────────┘ └───────────────┘
```

### Thread Safety Guarantees:
* **Capture Data Ownership**: Once a capture buffer is assembled by the `DeviceWorker`, ownership is passed to the `CaptureProcessor` via immutable `std::shared_ptr<const std::vector<uint8_t>>` or Qt implicitly shared data containers (`QByteArray`).
* **Waveform View Cache**: The `CaptureProcessor` generates a decimated Level-of-Detail cache. The GUI thread accesses only the pre-calculated display coordinates, never touching raw sample memory during paint events.
* **Non-Blocking Serial**: The `QSerialPort` lives completely within the `DeviceWorker` thread. Serial read/write buffers are flushed asynchronously via `readyRead()` and `bytesWritten()` events.

---

## 3. High-Performance Native Waveform Rendering Engine

### 3.1 The Level-of-Detail (LOD) Min/Max Decimation Algorithm
Capturing 65,536 samples (or multi-megasample future captures) and rendering individual line segments causes severe rendering stalls. The C++ Waveform Viewer solves this through a multi-resolution decimation pyramid:

```text
Raw Capture Buffer (65,536 Bytes)
               │
               ▼
┌──────────────────────────────────────────────┐
│  LOD Decimator (Capture Processor Thread)    │
│  Computes Min/Max State per Screen Pixel Col │
└──────────────────────┬───────────────────────┘
                       │
       ┌───────────────┴───────────────┐
       ▼                               ▼
Low Zoom (Full Span)           High Zoom (Detailed View)
• 1 Horizontal Pixel = 100 s   • 1 Horizontal Pixel = 1 s
• Rendered as Min/Max Box /    • Rendered as Exact Transition
  Activity Shade [ ||||| ]       Edges [ ──┐  ┌── ]
                                           └──┘
```

1. **Pixel-Bin Aggregation**: For a viewport width of $W$ pixels (e.g. 1920 px), the sample buffer is partitioned into $W$ temporal bins:
   $$\text{Samples per Bin} = \frac{\text{Visible Sample Count}}{W}$$
2. **Column Bit-Summary**: For each channel ($0..7$) within a bin:
   - If all samples in the bin are `LOW` $\rightarrow$ Render flat line at $Y = 0$.
   - If all samples in the bin are `HIGH` $\rightarrow$ Render flat line at $Y = 1$.
   - If both `LOW` and `HIGH` exist $\rightarrow$ Render vertical transition marker or solid activity block between $0$ and $1$.
3. **Sub-Pixel Efficiency**: Reduces painting complexity from $O(N)$ (where $N = 65,536$) to $O(W)$ (where $W \le 2560$), guaranteeing a locked **60+ FPS** render loop regardless of capture depth.

### 3.2 Dual Measurement Cursors & Edge Snapping
* **Cursors A & B**: Rendered as interactive vertical overlay lines with draggable grab handles.
* **Magnetic Edge Snapping**: When a cursor is dragged near a logic transition (within $\pm 5$ screen pixels), the cursor automatically snaps to the precise sample index of the rising or falling edge.
* **Telemetry HUD**: Displays real-time metrics:
  - $t_A$, $t_B$, and $\Delta t = |t_B - t_A|$
  - Instantaneous frequency $f = \frac{1}{\Delta t}$
  - Channel state at cursor position ($8$-bit bus value in binary and hexadecimal)
  - Number of rising/falling edges enclosed between Cursor A and Cursor B.

---

## 4. Device Abstraction & Dynamic Capability Model

To ensure the application easily supports future UDOO KEY firmware updates without GUI rewrites, the device interface is abstracted into a capability-driven architecture:

```cpp
enum class DeviceCapability : uint32_t {
    LogicAnalyzer   = 1 << 0,  // 8-channel 100 MSPS capture
    PrecisionMeter  = 1 << 1,  // Voltmeter, resistance, continuity
    HardwareI2C     = 1 << 2,  // I2C master & scanner
    HardwareSPI     = 1 << 3,  // SPI master & transaction builder
    HardwareUART    = 1 << 4,  // Serial bridge & monitor
    Dallas1Wire     = 1 << 5,  // 1-Wire search & DS18B20 reader
    DirectGPIO      = 1 << 6,  // Direct DIO pin control
    SumpProtocol    = 1 << 7,  // SUMP protocol compatibility
    UsbProbing      = 1 << 8,  // Physical USB D+/D- probing
    SensorSuite     = 1 << 9   // Autonomous multi-bus sensor scan
};
```

### 4.1 Device Life Cycle
```text
[ Disconnected ]
       │
       ▼  Enumerate serial ports & match VID/PID (0x2E8A)
[ Port Detected ]
       │
       ▼  Open port & transmit handshake (CMD_GET_INFO / SUMP_CMD_META)
[ Handshake OK ]
       │
       ▼  Negotiate capabilities & firmware metadata
[ Device Ready ] ──► GUI dynamically enables matching views & toolbars
       │
       ▼  Monitor connection heartbeat
[ Disconnected ] ──► Auto-release resources & prompt user
```

---

## 5. Proposed CMake Project Structure (`udoo-key-app`)

```text
udoo-key-app/
├── CMakeLists.txt                      # Root build configuration
├── CMakePresets.json                   # Presets for Windows (MSVC) & Linux (GCC)
├── README.md                           # Developer documentation
│
├── src/
│   ├── main.cpp                        # Application entry point
│   │
│   ├── app/                            # Application framework & lifecycle
│   │   ├── Application.h / .cpp
│   │   ├── AppContext.h / .cpp
│   │   ├── ThemeManager.h / .cpp       # Centralized Dark/Light/System theme engine
│   │   └── SettingsManager.h / .cpp    # QSettings-backed persistent configuration
│   │
│   ├── core/                           # Core domain logic & abstractions
│   │   ├── DeviceManager.h / .cpp      # Auto-detection & device enumeration
│   │   ├── DeviceSession.h / .cpp      # Active connection coordinator
│   │   ├── DeviceCapabilities.h        # Feature bitmasks & metadata
│   │   ├── EventBus.h / .cpp           # Thread-safe pub/sub event dispatcher
│   │   └── ErrorTypes.h                # Strongly-typed error definitions
│   │
│   ├── transport/                      # Physical I/O abstraction layer
│   │   ├── ITransport.h                # Pure virtual transport interface
│   │   ├── SerialTransport.h / .cpp    # QSerialPort implementation
│   │   └── MockTransport.h / .cpp      # Headless simulation & record/replay
│   │
│   ├── protocol/                       # Protocol framers & parsers
│   │   ├── IProtocolClient.h
│   │   ├── SumpClient.h / .cpp         # Standard SUMP client (PulseView parity)
│   │   ├── BinaryLogicClient.h / .cpp  # 0x5A UDOO high-speed binary protocol
│   │   └── TerminalClient.h / .cpp     # VT100/ASCII command bridge
│   │
│   ├── logic_analyzer/                 # Logic acquisition & signal model
│   │   ├── CaptureBuffer.h / .cpp      # 64 KiB SRAM ring-buffer management
│   │   ├── TriggerConfig.h             # Edge, level, pattern, runt width, pre-trigger %
│   │   ├── LodDecimator.h / .cpp       # Multi-resolution min/max decimation pyramid
│   │   └── SignalStatistics.h / .cpp   # Duty cycle, transition count, frequency estimation
│   │
│   ├── decoders/                       # Pluggable protocol decoders
│   │   ├── IDecoder.h                  # Abstract decoder interface
│   │   ├── DecoderModel.h / .cpp       # Virtualized QAbstractTableModel for event list
│   │   ├── UartDecoder.h / .cpp        # Configurable baud, data bits, parity, stop bits
│   │   ├── I2cDecoder.h / .cpp         # START/STOP, 7-bit ADDR, ACK/NACK, data
│   │   ├── SpiDecoder.h / .cpp         # Full-duplex MOSI/MISO with CPOL/CPHA
│   │   ├── OneWireDecoder.h / .cpp     # Reset, ROM code, scratchpad
│   │   └── UsbDecoder.h / .cpp         # NRZI, bit unstuffing, PID, CRC5/CRC16, SETUP
│   │
│   ├── export/                         # File exporter subsystem
│   │   ├── IExporter.h
│   │   ├── VcdExporter.h / .cpp        # IEEE 1364 Value Change Dump writer
│   │   ├── CsvExporter.h / .cpp        # Tabular CSV spreadsheet writer
│   │   └── SessionExporter.h / .cpp    # JSON complete project/capture session
│   │
│   └── ui/                             # Qt Graphical User Interface
│       ├── MainWindow.h / .cpp         # Central shell with dockable panels
│       │
│       ├── waveform/                   # Waveform rendering subsystem
│       │   ├── WaveformWidget.h / .cpp # Custom QPainter/LOD viewport canvas
│       │   ├── TimingCursorBar.h / .cpp# Cursors A/B, Delta-t, and frequency HUD
│       │   └── ChannelHeaderView.h     # Channel label, color picker, enable checkboxes
│       │
│       ├── panels/                     # Instrument-specific panels
│       │   ├── LogicPanel.h / .cpp     # Logic analyzer controls & toolbar
│       │   ├── MeterPanel.h / .cpp     # Precision voltmeter, resistance & continuity
│       │   ├── I2cPanel.h / .cpp       # Interactive 7-bit address scanner grid & R/W
│       │   ├── SpiPanel.h / .cpp       # SPI transaction builder & JEDEC interrogator
│       │   ├── UartPanel.h / .cpp      # Dedicated target serial terminal
│       │   ├── OneWirePanel.h / .cpp   # Dallas sensor list & temperature dashboard
│       │   ├── UsbProbePanel.h / .cpp  # USB electrical probe & packet sniffer view
│       │   ├── SensorSuitePanel.h/.cpp # Autonomous multi-bus sensor scan dashboard
│       │   ├── TerminalPanel.h / .cpp  # Embedded Bus Pirate interactive console
│       │   └── DecoderPanel.h / .cpp   # Decoded protocol transaction table view
│       │
│       └── dialogs/
│           ├── DeviceDialog.h / .cpp   # Connection & port configuration dialog
│           ├── TriggerDialog.h / .cpp  # Advanced trigger configuration editor
│           └── SettingsDialog.h / .cpp # Themes, font sizes, display preferences
│
├── tests/                              # Automated Unit & Integration Tests
│   ├── CMakeLists.txt
│   ├── test_protocol_framing.cpp       # Binary & SUMP packet parser tests
│   ├── test_lod_decimator.cpp          # Waveform decimation & zoom tests
│   ├── test_decoders.cpp               # UART, I2C, SPI decoder verification
│   └── test_mock_device.cpp            # Headless simulated UDOO KEY tests
│
└── resources/                          # Embedded Application Assets
    ├── icons/                          # SVG toolbar & status icons
    ├── themes/                         # Dark and Light QSS stylesheets
    └── udoo_app.qrc                    # Qt resource collection
```

---

## 6. Required Qt 6 Modules & Build Requirements

The application relies strictly on standard, officially maintained Qt 6 modules:

| Qt 6 Module | Purpose | CMake Link Target |
| :--- | :--- | :--- |
| **`Qt6::Core`** | Foundation types, event loop, threading, signals/slots, QSettings, timers. | `Qt6::Core` |
| **`Qt6::Gui`** | Canvas painting, 2D vector primitives, colors, fonts, image formats. | `Qt6::Gui` |
| **`Qt6::Widgets`** | Main window, dock widgets, splitters, tables, combo boxes, dialogs. | `Qt6::Widgets` |
| **`Qt6::SerialPort`** | Cross-platform serial communication on Windows COM ports and Linux `/dev/tty*`. | `Qt6::SerialPort` |
| **`Qt6::Concurrent`** | Background task execution (`QtConcurrent::run`) for decoders and exporters. | `Qt6::Concurrent` |
| **`Qt6::Test`** | Headless automated unit testing framework for CI/CD validation. | `Qt6::Test` |

---

## 7. Mock Device & Simulation Architecture

To enable complete software development, automated testing, and CI/CD validation without requiring physical UDOO KEY hardware, a comprehensive simulation backend (`MockTransport`) is integrated into the architecture:

```text
┌────────────────────────────────────────────────────────┐
│                      ITransport                        │
├──────────────────────────┬─────────────────────────────┤
│     SerialTransport      │        MockTransport        │
│    (Physical UDOO KEY)   │     (Headless Simulator)    │
│                          │                             │
│ • Windows COMx           │ • Synthetic 1 MHz Square    │
│ • Linux /dev/ttyACM0     │ • Simulated I2C Transactions│
│ • Real USB 1.1 CDC       │ • Simulated SPI JEDEC Read  │
│                          │ • Synthetic ADC Voltages    │
│                          │ • Configurable Error Inject │
└──────────────────────────┴─────────────────────────────┘
```

The simulator allows instant verification of:
1. Waveform zoom and pan rendering across 64 KiB buffers.
2. Timing cursor edge-snapping accuracy.
3. UART, I2C, and SPI protocol decoder correctness against known test vectors.
4. VCD and CSV export integrity.
5. Device disconnect, timeout, and reconnect error handling state machines.
