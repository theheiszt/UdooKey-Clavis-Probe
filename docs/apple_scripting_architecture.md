# Apple Scripting & Automation Architecture

## 1. Executive Summary & Design Principles

The Apple Scripting & Automation Extension provides a high-performance, non-blocking bridge allowing native macOS automation tools (AppleScript, JXA, Apple Events, Shortcuts) to orchestrate the **UDOO KEY Companion** native C++ desktop application. The system is designed with strict compile-time platform separation:
- **macOS Host**: Exposes Cocoa Scripting, `NSAppleScript`, `NSAppleEventManager`, and `.sdef` dictionary mapping Apple Events into cross-platform automation commands.
- **Windows & Linux Hosts**: Build cleanly without any Apple-specific dependencies or Objective-C headers, executing identical automation command semantics via CLI, JSON-RPC, and cross-platform automation backends.
- **Hardware Boundary**: The RP2040 and ESP32 microcontrollers remain platform-neutral instrumentation engines. Firmware executes hardware captures, triggers, voltage measurements, and debug protocols without knowing about AppleScript. Automation events create synchronized host event markers (`MARK_EVENT`) that correlate with microsecond/nanosecond hardware timestamps.

---

## 2. Multi-Tier Architecture & Layer Separation

```text
┌────────────────────────────────────────────────────────────────────────┐
│                   macOS AppleScript / Automation Tools                 │
│              (AppleScript, JXA, Shortcuts, Apple Events)                │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Apple Event ('UDOK') / Script
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                     Platform Automation Layer                          │
│  ┌───────────────────────────────┐ ┌────────────────────────────────┐  │
│  │  MacOSAppleAutomationBackend  │ │ Windows/LinuxAutomationBackend │  │
│  │   (Cocoa Scripting / .sdef)   │ │  (Cross-Platform Stubs & CLI)  │  │
│  └───────────────┬───────────────┘ └────────────────┬───────────────┘  │
└──────────────────┼──────────────────────────────────┼──────────────────┘
                   └────────────────┬─────────────────┘
                                    │ AutomationCommand & Params
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                    Cross-Platform Automation Core                      │
│                                                                        │
│   ┌───────────────────────────┐      ┌─────────────────────────────┐   │
│   │     AutomationEngine      │◄────►│    AutomationObjectModel    │   │
│   │ (Command Dispatch, Async) │      │ (App, Dev, Capture, Tracks) │   │
│   └──────────────┬────────────┘      └─────────────────────────────┘   │
│                  │                                                     │
│                  ├──────────────────────────────────┐                  │
│                  ▼                                  ▼                  │
│   ┌───────────────────────────┐      ┌─────────────────────────────┐   │
│   │ TimestampSynchronizer &   │      │  AppleUrlSessionCorrelator  │   │
│   │     TransactionLogger     │      │ (HTTP, DNS, TLS, WebSocket) │   │
│   │       [MARK_EVENT]        │      │                             │   │
│   └──────────────┬────────────┘      └─────────────────────────────┘   │
└──────────────────┼─────────────────────────────────────────────────────┘
                   │
                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│                       Application Subsystems                           │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐   │
│  │DeviceManager │ │LogicAnalyzer │ │  JTAG / SWD  │ │AppleDiagnostics│ │
│  │(Serial/Mock) │ │(Binary/SUMP) │ │ Debug Probes │ │(USB/BLE/WiFi)│   │
│  └──────┬───────┘ └──────┬───────┘ └──────┬───────┘ └──────┬───────┘   │
└─────────┼────────────────┼────────────────┼────────────────┼───────────┘
          └────────────────┼────────────────┼────────────────┘
                           │ Framed Binary Packets / UART / USB
                           ▼
┌────────────────────────────────────────────────────────────────────────┐
│                         UDOO KEY Hardware                              │
│         RP2040 (Logic/JTAG/ADC)  ◄──SPI/UART──►  ESP32 (BLE/WiFi)      │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Platform Separation Strategy

Apple scripting functionality is isolated behind the abstract `IAppleAutomationBackend` interface.

```cpp
namespace UdooKey {

class IAppleAutomationBackend {
public:
    virtual ~IAppleAutomationBackend() = default;
    virtual bool isAvailable() const = 0;
    virtual QString platformName() const = 0;
    virtual QString backendDescription() const = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual AutomationResult executeScript(const QString& scriptText) = 0;
    virtual AutomationResult handleAppleEvent(const QByteArray& eventData) = 0;
    virtual QVariant queryProperty(const QString& propertyPath) const = 0;
};

} // namespace UdooKey
```

### Backend Implementations:
1. **`MacOSAppleAutomationBackend`**:
   - Compiles only when targeting macOS (`#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)`).
   - Maps Apple Event 4-character codes (FourCC) like `'scap'` (Start Capture), `'conn'` (Connect), `'mark'` (Add Marker) into `AutomationCommandId`.
   - Evaluates AppleScript blocks and delegates commands to `AutomationEngine`.
2. **`WindowsAppleAutomationBackend`**:
   - Free of all Apple frameworks, Cocoa headers, and Objective-C runtimes.
   - `isAvailable()` returns `false` for Apple Events, but executes cross-platform commands seamlessly.
3. **`LinuxAppleAutomationBackend`**:
   - Lightweight POSIX backend without Apple dependencies.
4. **`UnavailableAppleAutomationBackend`**:
   - Safe fallback returning structured `AutomationErrorCode::PlatformUnavailable`.
5. **`AppleAutomationFactory`**:
   - Instantiates the active platform backend at runtime.

---

## 4. Cross-Platform Command & Parameter Model

Automation commands are strictly decoupled from GUI widgets. Scripts cannot simulate mouse clicks or directly mutate Qt widget states. Instead, commands target subsystem controllers.

### Core Command Enumeration (`AutomationCommandId`):
- **Device Operations**: `ConnectDevice`, `DisconnectDevice`, `GetDeviceStatus`, `GetDeviceInfo`
- **Acquisition & Logic Analyzer**: `StartCapture`, `StopCapture`, `ArmCapture`, `GetCaptureStatus`, `SetSampleRate`, `SetChannels`, `SetTrigger`
- **Hardware Debug Probes**: `StartJtag`, `StopJtag`, `GetJtagStatus`, `ResetTarget`, `StartSwd`, `StopSwd`, `GetSwdStatus`
- **Bluetooth Diagnostics**: `StartBleScan`, `StopBleScan`, `GetBleStatus`, `ConnectBleDevice`, `DisconnectBleDevice`, `ReadCharacteristic`, `WriteCharacteristic`
- **Network & Wi-Fi Diagnostics**: `StartWifiScan`, `StopWifiScan`, `GetWifiStatus`, `RunPing`, `RunDnsTest`, `RunTcpTest`
- **Apple Diagnostics**: `StartAppleSession`, `StopAppleSession`, `GetSessionStatus`, `ListAppleDevices`, `BootSimulator`, `ShutdownSimulator`, `CheckDeveloperMode`
- **Timeline & Correlation**: `AddTimelineMarker`, `ClearTimeline`, `GetTimelineCount`, `GetTimelineEvents`, `CorrelateEvent`, `RecordNetworkEvent`
- **Persistence & Export**: `SaveSession`, `LoadSession`, `ExportCapture`, `ExportReport`

---

## 5. Scriptable Object Model

The application exposes a structured, queryable hierarchy:

```text
Application (name, version, hostOS, scriptingAvailable)
│
├── Devices (portName, modelName, firmwareVersion, vendorId, productId, isConnected, capabilities)
│
├── Captures (state, sampleRateHz, sampleCount, channelCount, triggerChannel, triggerMode, duration)
│
├── Sessions (sessionId, description, isActive, startTime, eventCount)
│
├── Instruments
│   ├── LogicAnalyzer (isArmed, sampleRateHz, bufferDepth, triggerChannel, triggerEdge)
│   ├── JTAG (isRunning, detectedTaps, idCodes, manufacturer, clockFrequencyHz)
│   ├── SWD (isRunning, targetDpidr, targetPart, coreHalted, clockFrequencyHz)
│   ├── ADC (channel0Mv, channel1Mv, channel2Mv, channel0Volts, channel1Volts, channel2Volts)
│   ├── Bluetooth (isScanning, discoveredPeripherals, connectedDevice, rssiDbm)
│   └── WiFi (isScanning, interfaceName, ssid, bssid, rssiDbm, channel, ipAddress, txRateMbps)
│
└── Timeline (totalEvents, markerCount, lastEventSummary, lastEventTimestamp)
```

Properties can be queried via standard dot-notation path strings (e.g. `device.connected`, `instruments.logic.samplerate`, `adc.ch0v`, `timeline.count`).

---

## 6. Asynchronous Execution & Operation Lifecycle

Hardware operations (connecting, arming triggers, BLE/Wi-Fi scans, JTAG scans) can take variable time. The automation architecture handles them asynchronously without blocking the main event loop:

1. Client triggers an async command (`executeCommandAsync`).
2. `AutomationEngine` assigns a unique operation ID (e.g. `op_1042`).
3. An `AsyncOperationInfo` record is created with state `Running` and 0% progress.
4. Signals `commandStarted`, `commandProgress`, and `commandFinished` are emitted.
5. Clients query status (`getOperationStatus`) or cancel long operations (`cancelOperation`).

---

## 7. High-Precision UDOO Timestamp Correlation (`MARK_EVENT`)

When an automation script triggers an action, it inserts a host event marker:
```text
AppleScript Command
       │
       ▼
AutomationEngine::addTimelineMarker("TEST_STAGE_1")
       │
       ├─────────────────────────────────┐
       ▼                                 ▼
TimestampSynchronizer             TransactionLogger
(Monotonic Nanosecond Clock)      (Global Unified Timeline)
       │                                 │
       ▼                                 ▼
Correlated Hardware Event           Exported Diagnostics
(RP2040 Trigger Offset / Sample)    (HTML, JSON, CSV, VCD)
```

### Timeline Sequence Example:
```text
16:15:00.000000000 UTC  [Automation] [MARK_EVENT] SCRIPT_INIT
16:15:00.000120500 UTC  [Logic]      Capture Armed (10 MHz, 8 Channels, Trig GP0)
16:15:00.001450000 UTC  [Network]    HTTP GET https://api.apple.com/auth [42 ms]
16:15:00.005230000 UTC  [Hardware]   RP2040 Trigger Fired (Channel 0 Rising Edge)
16:15:00.010000000 UTC  [Automation] [MARK_EVENT] SCRIPT_COMPLETE
```

---

## 8. URLSession & Network Diagnostics Correlation

The `AppleUrlSessionCorrelator` maps application-level network events into the unified timeline:
- HTTP Requests and HTTP Responses (with response status codes, latencies, and payload byte counts)
- DNS resolution times and resolved IP addresses
- TCP connection setup and handshake durations
- TLS handshakes and negotiated cipher suites
- WebSocket frame transfers and binary payloads
- Network timeouts and socket errors

---

