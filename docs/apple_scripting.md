# AppleScript & Automation User Guide

## Overview

The **UDOO KEY Companion** application provides rich AppleScript, Apple Events, and cross-platform automation capabilities for orchestrating hardware instrumentation, logic analyzer captures, JTAG/SWD debugging, and Apple device diagnostics.

---

## 1. AppleScript Terminology & Quick Start

On macOS, you can control the application using AppleScript scripts or from the command line using `osascript`:

```applescript
tell application "UDOO KEY Companion"
    -- 1. Connect to UDOO hardware or virtual test engine
    connect device mock true

    -- 2. Configure Logic Analyzer
    set sample rate to 20000000
    set channel count to 8
    set trigger channel to 0
    set trigger mode to "Rising"

    -- 3. Mark timeline and arm capture
    add marker label "AUTOMATION_START"
    arm capture

    -- 4. Start Apple Diagnostic Session
    start apple session description "Automated iPhone USB & Power Trace"

    -- 5. Trigger capture
    start capture

    -- 6. Export trace & diagnostics
    export capture to "/tmp/capture.vcd" format "vcd"
    export report to "/tmp/report.html" format "html"
    save session to "/tmp/test_session.udoo"

    -- 7. Finish
    add marker label "AUTOMATION_COMPLETE"
    disconnect device
end tell
```

---

## 2. Command Reference

### Device Management

| Command | Parameters | Description |
| :--- | :--- | :--- |
| `connect device` | `port` (text), `baud rate` (integer, default 921600), `mock` (boolean) | Connects to physical hardware or mock simulator. |
| `disconnect device` | *(none)* | Disconnects the active device. |
| `get device status` | *(none)* | Returns connection state, hardware model, firmware revision. |
| `get device info` | *(none)* | Returns full capabilities, VID/PID, and manufacturer string. |

### Logic Analyzer & Capture

| Command | Parameters | Description |
| :--- | :--- | :--- |
| `start capture` | `sample rate` (int), `depth` (int), `trigger channel` (int), `trigger edge` (text) | Starts acquisition immediately or after trigger. |
| `stop capture` | *(none)* | Aborts active capture. |
| `arm capture` | `sample rate` (int), `trigger channel` (int) | Arms hardware waiting for trigger event. |
| `get capture status` | *(none)* | Returns state (Idle, Armed, Triggered, Complete), rate, and sample count. |

### Timeline & Host Event Markers

| Command | Parameters | Description |
| :--- | :--- | :--- |
| `add marker` | `label` (text), `details` (text, optional) | Inserts a nanosecond-timestamped marker into the unified timeline. |
| `clear timeline` | *(none)* | Clears logged transactions and event markers. |

### Hardware Debug Probes (JTAG & SWD)

| Command | Parameters | Description |
| :--- | :--- | :--- |
| `start jtag` | `frequency` (int, default 1000000) | Scans JTAG TAP chain and returns IDCODEs. |
| `stop jtag` | *(none)* | Halts JTAG operations. |
| `start swd` | `frequency` (int, default 1000000) | Connects to ARM CoreSight SWD target and reads DPIDR. |
| `stop swd` | *(none)* | Halts SWD operations. |
| `reset target` | `force` (boolean, required for security) | Asserts system reset line to attached microcontroller. |

### Wireless & Network Diagnostics

| Command | Parameters | Description |
| :--- | :--- | :--- |
| `start ble scan` | *(none)* | Scans for Apple Bluetooth Low Energy peripherals. |
| `stop ble scan` | *(none)* | Stops active BLE scan. |
| `start wifi scan` | *(none)* | Gathers Wi-Fi interface telemetry (SSID, RSSI, channel, Tx rate). |
| `stop wifi scan` | *(none)* | Stops Wi-Fi diagnostics. |
| `run ping` | `target` (text, e.g. "apple.com") | Performs ping latency and packet loss test. |
| `run dns test` | `domain` (text) | Measures DNS query resolution time. |
| `run tcp test` | `host` (text), `port` (int) | Tests TCP socket connectivity. |

### Apple Diagnostics & Sessions

| Command | Parameters | Description |
| :--- | :--- | :--- |
| `start apple session` | `description` (text) | Begins a unified correlation session. |
| `stop apple session` | *(none)* | Finalizes active session. |
| `export capture` | `to` (text path), `format` ("vcd", "csv", "bin") | Exports logic analyzer waveform data. |
| `export report` | `to` (text path), `format` ("html", "json", "csv") | Generates full diagnostics report. |
| `save session` | `to` (text path, e.g. "trace.udoo") | Serializes complete session state to `.udoo` bundle. |
| `load session` | `from` (text path) | Restores saved session from `.udoo` file. |

---

## 3. Querying Properties & Object Model

You can read properties through standard dot-notation paths:

```applescript
tell application "UDOO KEY Companion"
    -- Check if hardware is connected
    set isConn to get property "device.connected"
    
    -- Check sample rate
    set curRate to get property "capture.samplerate"
    
    -- Read analog voltage (ADC GP26)
    set v0 to get property "adc.ch0v"
    
    -- Read Wi-Fi SSID and RSSI
    set curSsid to get property "wifi.ssid"
    set curRssi to get property "wifi.rssi"
    
    -- Read total logged events
    set evtCount to get property "timeline.count"
end tell
```

---

## 4. Structured Error Model

Commands return structured error records when a failure occurs:

```json
{
  "code": 1,
  "codeString": "DeviceNotConnected",
  "message": "No UDOO device is currently connected.",
  "recommendation": "Invoke 'connect device' prior to executing hardware commands."
}
```

### Error Code Summary:
- `DeviceNotConnected`: USB port is closed or disconnected.
- `InvalidParameter`: Parameter is missing, out of range, or invalid.
- `CaptureAlreadyActive`: Attempted to start or arm while capture is in progress.
- `PermissionDenied`: Protected or destructive operation attempted without explicit confirmation flag.
- `PlatformUnavailable`: Apple-specific macOS feature invoked on Windows/Linux host.
- `AppleToolingUnavailable`: Xcode developer tools (`devicectl`, `simctl`) are not installed.

---

