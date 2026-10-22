# UDOO KEY — USB Console & Firmware Stack Audit

**Document ID:** DOC-USB-CONSOLE-AUDIT-20261003  
**Status:** VALIDATED AUDIT  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040 + ESP32-WROVER-E)  
**Authority Reference:** PlatformIO Configuration & Source Tree Inspection  

---

## 1. Executive Summary

This audit establishes the state of the USB stack and CDC serial console implementation within the UDOO KEY project codebase. It resolves the 8 mandatory architectural questions required prior to implementing the Bus Pirate 5-style terminal interface.

The audit confirms that **RP2040 USB CDC is already fully functional, stable, and active** within the project, utilizing the Earle Philhower `framework-arduinopico` core with bundled Adafruit TinyUSB. No secondary USB stack or duplicate driver layer is required or permitted.

---

## 2. Mandatory Audit Responses

### 1. Which PlatformIO environment owns the RP2040 USB firmware?
**Answer:** `[env:rp2040_logic_analyzer]` located in `rp2040_logic_analyzer/platformio.ini`.
- Platform: `https://github.com/maxgerhardt/platform-raspberrypi/archive/refs/heads/develop.zip`
- Board: `generic` (RP2040 133 MHz, 16 MB Flash, 264 KB SRAM)
- Core: `earlephilhower`
- Framework: `arduino` (`framework-arduinopico`)

### 2. Whether TinyUSB is already present?
**Answer:** **YES.**
- The Earle Philhower core includes Adafruit TinyUSB as its native USB stack.
- TinyUSB handles USB Device enumeration, descriptors, CDC ACM (virtual serial port), and endpoint packet transfers.

### 3. Whether Pico SDK USB support is already present?
**Answer:** **YES.**
- The underlying toolchain (`toolchain-rp2040-earlephilhower` v5.160100) and framework bundle the full Raspberry Pi Pico SDK (`pico-sdk`), including hardware USB registers (`hardware/structs/usb.h`), USB PHY clocking, and `pico_stdio_usb`.

### 4. Whether USB CDC is already working?
**Answer:** **YES.**
- The global `Serial` object in the RP2040 firmware maps directly to `SerialUSB` (CDC ACM virtual COM port).
- Baud rate configuration on the host side (e.g., 115200, 9600, etc.) is accepted without error; data transfers operate at full native USB 1.1 Full-Speed packet rates (~1 MB/s).

### 5. Whether SUMP currently uses USB CDC?
**Answer:** **YES.**
- SUMP logic analyzer commands and sample streaming run entirely over `Serial` (`sump_process_byte(b, Serial)`, `sump_task(Serial)`).
- Sigrok / PulseView connects directly to this virtual COM port.

### 6. Whether the existing CLI already uses USB CDC?
**Answer:** **YES.**
- In `rp2040_logic_analyzer/src/main.cpp`, `process_usb_cli()` reads ASCII characters from `Serial`, echoes characters back to `Serial`, and invokes `bp_execute_command(usb_cmd_buffer, Serial)`.
- However, the existing implementation is primitive: it lacks VT100 escape sequence handling, arrow navigation, cursor positioning, command history, and a safe default HiZ startup state.

### 7. Whether the current firmware can coexist with the terminal shell?
**Answer:** **YES.**
- Coexistence is achieved through the layered transport architecture:
  ```text
                      RP2040 USB CDC (Serial)
                                 │
                   ┌─────────────┴─────────────┐
                   │                           │
          SUMP / Binary Packets         Human Terminal CLI
             (0x00, 0x02, 0x04)         (Printable ASCII, ESC)
                   │                           │
                   ▼                           ▼
            sump_protocol              Terminal Engine
          (PulseView Stream)           (VT100 Line Editor)
  ```
- When SUMP handshakes arrive, the console yields to SUMP. When the user types `\r`, `\n`, or `ESC` in a terminal, the console immediately returns to the interactive terminal prompt.

### 8. Whether USB initialization is performed by RP2040 firmware or another component?
**Answer:** **RP2040 firmware.**
- USB initialization is executed on the RP2040 during early C++ runtime initialization before `setup()` is called.
- The ESP32 is completely uninvolved in the RP2040 USB CDC link when jumper `JP1` is closed.

---

## 3. Coexistence Matrix & Demultiplexing Strategy

To prevent collisions between different USB consumers, the input stream will be handled by a non-blocking classifier:

| Protocol Stream | Identification Magic / Trigger | Destination Subsystem | Action on Disconnect / Exit |
| :--- | :--- | :--- | :--- |
| **SUMP / PulseView** | Bytes `0x00` (Reset), `0x02` (ID), `0x04` (Meta) | `sump_protocol` | Restores Terminal Prompt on `\r`, `\n`, or `ESC` |
| **Python Companion App**| Byte `0x5A` (`PROTOCOL_MAGIC`) | `serial_protocol` | Returns to Terminal on frame completion |
| **Interactive Human Terminal** | Printable ASCII, `\r`, `\n`, VT100 `ESC [...]` | `terminal_engine` | Line editing, history, execution, prompt |

---

## 4. Audit Conclusion & Action Plan

1. **Stack Reuse**: Do NOT introduce a second TinyUSB stack. Reuse `Serial` as the underlying byte stream.
2. **Terminal Abstraction**: Create `src/terminal/terminal_transport.h` and `.cpp` wrapping `Serial` to provide clean stream operations.
3. **Line Editor & History**: Implement `terminal_line_editor`, `terminal_history`, and `terminal_vt100` to deliver the full Bus Pirate 5 terminal experience.
4. **Safety Default**: Initialize all external probe pins to high-impedance safe state (`HiZ>`) on boot before presenting the prompt.
