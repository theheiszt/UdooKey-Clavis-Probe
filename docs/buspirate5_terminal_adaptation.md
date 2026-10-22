# UDOO KEY — Bus Pirate 5 Terminal Software Adaptation

**Document ID:** DOC-BP5-ADAPT-20261003  
**Status:** IMPLEMENTED (Milestone 1)  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040)  

---

## 1. Algorithmic Adaptation Strategy

The UDOO KEY terminal interface adopts the user experience, command conventions, and input editing concepts of Bus Pirate 5 while maintaining complete architectural independence from BP5-specific hardware.

### 1.1 What Is Adapted:
- **Linenoise-Style Non-Blocking Line Editor**:
  - Incremental character-by-character processing.
  - In-place line editing with left/right cursor movements.
  - History buffer recalling previous commands with Up/Down arrow keys.
- **VT100 Escape Parser**:
  - Decoding multi-byte ANSI sequences (`ESC [ A`, `ESC [ B`, etc.).
  - ANSI colors for prompts, errors, warnings, and numbers.
  - Automatic ASCII fallback when plain serial terminals are detected.
- **Structured Command Dispatch**:
  - Trimming whitespace, lowercase command matching, space-delimited argument parsing.
- **Safety First (`HiZ>`) Prompt**:
  - Always boots into high-impedance mode with target pins safe.

### 1.2 What Is Replaced with UDOO HAL:
- **USB Layer**: Replaces BP5 custom USB FIFO rings with the unified `terminal_transport` over Arduino-Pico USB CDC.
- **GPIO & Pin Driving**: Replaces BP5 74HCT level shifters with direct RP2040 SIO / PIO registers and `InstrumentModeManager`.
- **ADC & Power**: Replaces BP5 programmable power supply and onboard ADC with the UDOO KEY 12-bit SAR ADC subsystem and RT6150 buck-boost rail.
- **Displays**: Replaces BP5 SPI LCD with the onboard ST7735 128x128 color TFT driver.

---

## 2. Feature Implementation Status Matrix

| Subsystem Component | Module Location | Status | Validation Method |
| :--- | :--- | :--- | :--- |
| **USB CDC Transport Abstraction** | `src/terminal/terminal_transport.*` | **IMPLEMENTED** | Verified against Serial CDC stream |
| **VT100 Sequence Generator & Parser** | `src/terminal/terminal_vt100.*` | **IMPLEMENTED** | Tera Term / PuTTY ANSI compliance |
| **Line Editor (Arrows, BS, Del, Home/End)** | `src/terminal/terminal_line_editor.*` | **IMPLEMENTED** | In-place cursor and history scrolling |
| **RAM Command History Buffer (8 slots)** | `src/terminal/terminal_history.*` | **IMPLEMENTED** | Up/Down navigation tested |
| **Safe Default HiZ Mode (`HiZ>`)** | `src/terminal/terminal.*` | **IMPLEMENTED** | Boot state verified on GPIOs |
| **Command Lexer & Dispatcher (`?`, `m`, etc.)** | `src/terminal/terminal.*` | **IMPLEMENTED** | Core commands verified |
| **SUMP Coexistence & Auto-Yield** | `src/main.cpp` + `src/terminal/` | **IMPLEMENTED** | Seamless PulseView handover |
| **Logic Analyzer Command Wrapper** | `src/terminal/terminal.*` | **IMPLEMENTED** | Calls frozen PIO/DMA capture engine |
| **Multifunction Meter Command Wrapper** | `src/terminal/terminal.*` | **IMPLEMENTED** | Calls ADC voltage & continuity APIs |
| **Protocol Modes (SPI, I2C, UART, 1-Wire)** | `src/bus_pirate_core.*` | **IMPLEMENTED** | Hardware master protocols |
| **USB Sniffer CLI Wrapper** | `src/terminal/terminal.*` | **PLANNED** | Scheduled for Phase USB-1 |
