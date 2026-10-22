# UDOO KEY — Python Companion Application Audit

**Document ID:** DOC-APP-AUDIT-20261003  
**Status:** COMPLETE (PHASE 1 AUDIT)  
**Date:** 2026-10-03  
**Target Application:** `python_companion_app` (PyQt6 + PyQtGraph + PySerial)  
**Companion Hardware:** SECO UDOO KEY (RP2040 Dual Cortex-M0+ @ 133 MHz, REV O)  

---

## 1. Executive Summary

This document establishes a complete, forensic engineering audit of the existing Python companion application (`python_companion_app/`). It catalogs all user interface components, serial/transport implementations, binary and ASCII protocol interactions, waveform visualization methods, protocol decoders, data export routines, dependencies, and threading behaviors.

The objective is to establish an authoritative behavioral and protocol baseline to guide the design and implementation of the **native cross-platform C++ (Qt 6 + CMake) desktop application** for Windows 10/11 and Linux, ensuring that all valuable capabilities are retained, architectural shortcomings are resolved, and no regressions are introduced against the verified UDOO KEY firmware.

---

## 2. File Inventory & Dependency Manifest

The Python application consists of three primary modules and one dependencies manifest:

```text
python_companion_app/
├── requirements.txt            # Dependency specification
├── serial_driver.py            # Serial transport, packet framing & worker thread (191 lines)
├── protocol_decoders.py        # Algorithmic decoders for UART, I2C, and SPI (244 lines)
└── logic_analyzer_gui.py       # Main PyQt6 GUI window, plotting & terminal (575 lines)
```

### 2.1 Dependencies Manifest (`requirements.txt`)

| Dependency | Specified Version | Purpose in Python App | Evaluation for C++ Migration |
| :--- | :--- | :--- | :--- |
| **`PyQt6`** | `>= 6.5.0` | Desktop GUI widgets, layouts, signals/slots, event loop. | Replaced by native **Qt 6 C++** (`QtWidgets`, `QtCore`, `QtGui`). Zero runtime interpreter overhead. |
| **`pyqtgraph`** | `>= 0.13.3` | Multi-channel digital waveform plotting and timing cursors. | Replaced by custom **C++ GPU/QPainter high-performance Waveform Widget** with Level-of-Detail (LOD) decimation. |
| **`pyserial`** | `>= 3.5` | Serial port enumeration, baud rate configuration, I/O. | Replaced by **`QSerialPort`** (`QtSerialPort` module) with asynchronous event-driven I/O. |
| **`numpy`** | `>= 1.24.0` | Bit-mask vectorization, sample buffer conversion, time linspace. | Replaced by standard **`std::vector<uint8_t>`**, bitwise SIMD/C++ operations, and fixed-point math. |

---

## 3. Comprehensive Feature & Subsystem Audit Matrix

| Feature | Python Implementation | Input | Output | Protocol | Hardware Dep. | UI Dep. | Threading Behavior | Limitations | Migration Recommendation |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Serial Enumeration** | `serial.tools.list_ports.comports()` in `serial_driver.py` | OS COM query | String list of device names | None | Host OS USB/Serial stack | `QComboBox` port selector | Synchronous on UI thread | Blocks UI if port query hangs; does not identify UDOO KEY VID/PID. | **REDESIGN**: Use `QSerialPortInfo` with automatic VID/PID matching (`0x2E8A` RP2040) and auto-discovery. |
| **Device Connection** | `serial.Serial(port, baudrate, timeout=0.1)` | User port selection | Connected status boolean | None | UDOO KEY USB CDC | Connect/Disconnect button | Creates background `Thread(_rx_loop)` | Timeout-based polling in thread; lacks robust reconnection handshake. | **REDESIGN**: Asynchronous `QSerialPort` in dedicated `DeviceSession` worker thread. |
| **Terminal / CLI Transport** | `send_command(cmd_str)` and `_rx_loop` ASCII path | User string input from text box | Text stream appended to `QTextEdit` | ASCII / VT100 text | UDOO KEY USB CDC | `QLineEdit`, `QTextEdit` console | Background thread emits PyQt signal | Ambiguity: `0x5A` ('Z') in ASCII text can be misidentified as binary magic byte. | **REDESIGN**: Deterministic frame discriminator separating binary packets from ASCII terminal streams. |
| **Logic Config Dispatch** | `set_config(rate, count, trig_ch, trig_mode)` | Rate combo, sample count spinbox | 8-byte payload: `<IIBB` | Binary `0x5A` `CMD_SET_CONFIG (0x02)` | PIO Capture engine | Rate & sample widgets | Background write via driver | Hardcoded trigger channel (0) and mode (0); no pre-trigger or width config in GUI. | **KEEP & EXPAND**: Full GUI trigger setup (rising, falling, any, pattern, runt width, pre-trigger %). |
| **Capture Triggering** | `start_capture()` and `send_bp_cmd("start")` | Start button click | Command `0x03` + ASCII `"start"` | Dual binary + ASCII | PIO0 SM0 DMA Circular Engine | Start / Stop buttons | Non-blocking command dispatch | Redundant dual dispatch (binary command + CLI string) needed due to firmware mode ambiguity. | **REDESIGN**: Clean session state machine selecting Binary Protocol, SUMP mode, or CLI mode explicitly. |
| **Capture Data Streaming** | `CMD_STREAM_DATA (0x05)` parser in `_rx_loop` | Raw bytes from serial | Emits `(data_bytes, sample_rate)` | Binary `0x5A 0x05` + `<II` header | RP2040 DMA circular ring | Signal to GUI | Blocking `_read_exact` with 5s timeout in background thread | If packet drops a byte, framing loses sync until timeout expires; buffers up to 128 KiB. | **REDESIGN**: Robust stateful stream deframer with CRC / header validation and ring-buffer recovery. |
| **Waveform Rendering** | `on_data_received` using PyQtGraph in `logic_analyzer_gui.py` | Raw 8-bit sample bytearray | 8 vertically stacked digital traces | Internal numpy array | None (GUI display) | `pg.PlotWidget`, `pg.PlotCurveItem` | Main UI thread executes numpy vectorization & plotting | Python GIL stalls; creating 8 `PlotCurveItem` objects on every capture causes UI stutter on 64 KiB. | **REDESIGN**: Native C++ Waveform Widget using Level-of-Detail (LOD) min/max decimation and GPU/QPainter. |
| **Timing Cursors** | `cursor_line_a`, `cursor_line_b` (`pg.InfiniteLine`) | User mouse drag | Time labels ($t_A, t_B, \Delta t, f$) | None | None | Cursor readout bar | Main UI thread signal callback | Basic 2-cursor line; cannot snap to signal edges; no cursor difference table. | **KEEP & EXPAND**: Dual measurement cursors with magnetic edge snapping, pulse measurement, and markers. |
| **UART Protocol Decoder** | `decode_uart()` in `protocol_decoders.py` | Raw byte buffer, sample rate, RX channel, baud | List of `{sample_idx, timestamp, hex, ascii}` | Algorithmic 8N1 bit-sampling | None | `QTableWidget` table rows | Synchronous on UI thread | Freezes UI during long captures; supports only 8N1; no parity or stop-bit customization. | **REDESIGN**: Background worker thread decoder with configurable framing (data bits, parity, stop bits). |
| **I2C Protocol Decoder** | `decode_i2c()` in `protocol_decoders.py` | Raw byte buffer, SCL ch, SDA ch | List of START, ADDR, DATA, ACK, STOP events | Algorithmic edge & level detection | None | `QTableWidget` table rows | Synchronous on UI thread | Slow loop in Python; cannot handle clock stretching or repeated start without glitches. | **REDESIGN**: High-performance C++ state machine decoder with transaction grouping and address lookup. |
| **SPI Protocol Decoder** | `decode_spi()` in `protocol_decoders.py` | Raw byte buffer, SCK, MOSI, MISO, CS, CPOL/CPHA | List of CS events, TX/RX hex bytes, ASCII | Algorithmic clock-edge sampling | None | `QTableWidget` table rows | Synchronous on UI thread | Table insertion limited to 500 rows to prevent Python UI lockup; misses deep capture transactions. | **REDESIGN**: Virtualized table model (`QAbstractTableModel`) allowing millions of decoded transactions. |
| **VCD File Export** | `export_vcd()` in `logic_analyzer_gui.py` | Captured sample buffer | ASCII `.vcd` file on disk | Value Change Dump (IEEE 1364) | None | `QFileDialog` | Main UI thread file loop | Iterates through up to 65,536 samples in Python string formatting; blocks UI during save. | **REDESIGN**: Asynchronous stream writer writing optimized transition-only VCD records in background. |
| **CSV File Export** | `export_csv()` in `logic_analyzer_gui.py` | Captured sample buffer | Comma-separated spreadsheet | Tabular CSV | None | `QFileDialog` | Main UI thread file loop | Formats every single sample line-by-line; creates multi-megabyte files; blocks UI. | **REDESIGN**: High-throughput buffered CSV / binary export running in background thread. |
| **Quick Action Buttons** | Top toolbar buttons: `I2C Scan`, `SPI Flash`, `SWD`, `Voltages`, `Self Test`, `Freq Sweep`, `100 MSPS`, `Displays` | Button click | Transmits string to Bus Pirate CLI | ASCII CLI commands | RP2040 subsystems | Toolbar & terminal | Dispatches via `send_command` | Unidirectional text trigger; does not parse output into structured GUI controls. | **REDESIGN**: Dedicated instrument views (SPI Builder, I2C Grid, Voltmeter Panel) parsing telemetry directly. |

---

## 4. Analysis of Python Architectural Limitations

### 4.1 Python Global Interpreter Lock (GIL) & Worker Threading
* In the Python app, `_rx_loop` runs in a `threading.Thread`, but transferring the byte stream into numpy and emitting Qt signals requires the Python GIL.
* When receiving a 64 KiB buffer at high repetition rates, the UI thread drops frames, causing cursor lag and unresponsive buttons.
* Protocol decoding executes entirely on the main UI thread during `on_data_received()`. Decoding a 64 KiB SPI transfer with thousands of clock transitions stalls the UI for 200–500 ms.

### 4.2 Waveform Rendering Inefficiencies
* PyQtGraph creates separate Python objects for every trace and point.
* The Python app completely clears and recreates the plot items (`self.plot_widget.clear()`, re-adding cursors and re-plotting 8 channels) on **every single capture**.
* This triggers massive garbage collection cycles and memory thrashing.
* Zooming and panning at low zoom levels forces the graphics engine to iterate through tens of thousands of coordinates without hierarchical level-of-detail (LOD) downsampling.

### 4.3 Protocol Collision & Stream Framing Ambiguity
* In `_rx_loop()` in `serial_driver.py`:
  ```python
  if raw_byte[0] == PROTOCOL_MAGIC: # 0x5A ('Z')
      cmd_byte = self.ser.read(1)
      if cmd in (CMD_GET_INFO, CMD_SET_CONFIG, CMD_START_CAPTURE, CMD_STOP_CAPTURE, CMD_STREAM_DATA):
          # Binary packet processing...
  ```
* If the user types a command containing uppercase 'Z' followed by an ASCII character whose byte value happens to be 1..5 (e.g., Ctrl-A, Ctrl-B, or specific terminal escapes), the parser erroneously treats it as binary and discards or corrupts data.
* If a binary packet drops a byte during USB CDC transmission, `_read_exact()` blocks for up to 5 seconds, freezing data acquisition.

### 4.4 Incomplete Instrument Coverage
* While the Python app provides quick buttons for `I2C Scan` and `Voltages`, it merely dumps the raw ASCII text into the terminal text box.
* It does not provide:
  - An interactive I2C address grid with clickable read/write operations.
  - A structured multi-channel precision voltmeter display with bar graphs or dial gauges.
  - An SPI transaction builder with hexadecimal hex/ASCII payload editors.
  - Dallas 1-Wire sensor enumeration or live temperature telemetry displays.
  - USB bus state and physical probing inspection panels.
  - NTC thermistor calibration controls.

---

## 5. Reusable Behavior & Assets to Preserve

The native C++ application must retain and improve upon these proven behavioral patterns:

1. **Digital Channel Layout**: Vertically stacked digital channels (Ch0..Ch7) with distinct high-contrast traces (Green, Cyan, Yellow, Orange, Magenta, Purple, Blue, White) on a dark slate background (`#0f172a`).
2. **Dual Measurement Cursors**: Two interactive draggable timing cursors (Cursor A and Cursor B) with real-time display of:
   - Absolute timestamp of Cursor A and Cursor B.
   - Time delta ($\Delta t = |t_B - t_A|$).
   - Frequency equivalent ($f = 1 / \Delta t$) formatted dynamically in Hz, kHz, or MHz.
3. **Multi-Protocol Decoding**:
   - UART: Configurable RX channel, standard baud rates (9600 to 2 Mbps), LSB-first 8N1 bit extraction.
   - I2C: Clocked on SCL rising edge, START / RESTART / STOP detection, 7-bit address extraction with R/W flag, ACK/NACK validation, payload hex/ASCII display.
   - SPI: Clocked on SCK edges according to CPOL/CPHA mode, Chip Select assertion/deassertion boundaries, full-duplex MOSI and MISO extraction.
4. **Data Export Compatibility**:
   - IEEE 1364 Value Change Dump (`.vcd`) export with proper timescale header (`$timescale 10ns $end`) and wire definitions (`$var wire 1 ch0 Ch0 $end`).
   - Tabular CSV export with timestamps, individual channel binary states, and aggregate hex bytes.
5. **Integrated Universal Terminal**:
   - Embedded interactive serial terminal console supporting direct Bus Pirate syntax execution, command history, and clear logging.

---

## 6. Audit Conclusion & Migration Mandate

The Python application serves as a functional proof-of-concept for basic logic capture, PyQtGraph visualization, and elementary decoders. However, its architectural coupling, GIL bottlenecks, UI-thread decoding, fragile stream demultiplexing, and lack of structured instrument panels make it unsuitable for professional bench testing.

The native C++ application will not be a line-by-line translation. Instead, it will be engineered around:
1. An asynchronous, multi-threaded `QSerialPort` device layer with automatic UDOO KEY discovery.
2. A high-performance native C++ Waveform Viewer with LOD decimation and edge snapping.
3. Background protocol decoders backed by virtualized Qt table models.
4. Dedicated graphical instrument panels for Logic Analyzer, Voltmeter, SPI Builder, I2C Grid, UART Terminal, 1-Wire, and USB Probing.
5. Cross-platform CMake build configuration supporting Windows MSVC and Linux GCC/Clang with zero Python dependencies.
