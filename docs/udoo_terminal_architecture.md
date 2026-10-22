# UDOO KEY — Universal Terminal Architecture & Engine Design

**Document ID:** DOC-TERM-ARCH-20261003  
**Status:** IMPLEMENTED (Milestone 1)  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040)  

---

## 1. Architectural Overview

The UDOO KEY Universal Terminal Subsystem elevates the USB CDC serial connection into a first-class, standalone instrument operating environment. Following the engineering philosophy of Bus Pirate 5, the instrument requires no proprietary PC client or GUI software. Users interact with the instrument via standard, cross-platform terminal emulators (Tera Term, PuTTY, minicom, screen).

### Subsystem Layer Stack
```text
┌─────────────────────────────────────────────────────────────┐
│                 USB Host (Tera Term / PuTTY)                 │
└──────────────────────────────┬──────────────────────────────┘
                               │ Virtual COM Port (115200 8N1)
                               ▼
┌─────────────────────────────────────────────────────────────┐
│              Hardware USB CDC (RP2040 TinyUSB)              │
├─────────────────────────────────────────────────────────────┤
│         Terminal Transport (terminal_transport.h/.cpp)       │
│  terminal_available(), terminal_read(), terminal_write()    │
├─────────────────────────────────────────────────────────────┤
│             VT100 Engine (terminal_vt100.h/.cpp)            │
│  Cursor manipulation, screen erase, color presets, fallback │
├─────────────────────────────────────────────────────────────┤
│        Line Editor & History (terminal_line_editor.h/.cpp)   │
│  Arrow navigation, backspace/del, home/end, history buffer  │
├─────────────────────────────────────────────────────────────┤
│        Command Lexer & Dispatcher (terminal.h/.cpp)         │
│  Whitespace trimming, argument tokenizing, command lookup   │
├─────────────────────────────────────────────────────────────┤
│             Mode Manager (InstrumentModeManager)            │
│  Safe transition: HiZ, Logic, SPI, I2C, UART, Meter, USB    │
├─────────────────────────────────────────────────────────────┤
│              Hardware Abstraction Layer (HAL)               │
│  RP2040 SIO, PIO0, DMA Ch0, 12-bit ADC, Timer/PWM           │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Startup Sequence & Electrical Safety

Safety is paramount: target driving pins must **never** power up in an active or floating driven state.

```text
Power Applied / Reset Vector
            │
            ▼
1. Hardware Safety Initialization:
   - Configure GP2..GP9 (Probe Bus) as high-impedance inputs (HiZ, no pull-ups).
   - Configure GP10 (EXT TRIG) as input pulldown.
   - Configure GP14 (Prescaler) as input.
   - Configure GP26..GP28 (ADC0..ADC2) as analog inputs.
            │
            ▼
2. Initialize Subsystems:
   - ModeMgr.enterMode(InstrumentMode::IDLE) -> HiZ confirmed.
   - AdcSubsystem initialized.
   - UI / Displays initialized.
            │
            ▼
3. Initialize USB CDC & Terminal Transport:
   - Initialize terminal line editor and history buffer.
   - Set default mode to HiZ.
            │
            ▼
4. Output Startup Banner:
   - Clear line / Reset terminal attributes.
   - Display UDOO KEY banner, version, build date, and safe mode prompt.
            │
            ▼
5. Present Prompt:
   "HiZ> "
```

---

## 3. Disconnect & Reconnect State Machine

The terminal interface treats USB disconnection as a communication event, **not a hardware reboot**:
- If DTR drops or the USB cable is unplugged:
  - All active bus transactions halt safely.
  - Active driven outputs in protocol modes are immediately brought to a safe high-impedance state.
- Upon reconnection:
  - Terminal transport resets line buffer state.
  - Terminal prints the welcome banner and prompt: `HiZ> `.
  - No active bus drive is resumed without explicit user commands.

---

## 4. VT100 / ANSI vs ASCII Fallback Modes

The terminal engine supports both rich VT100 terminals and dumb/raw ASCII terminals:

| Feature | VT100 Mode (Default) | ASCII Fallback Mode |
| :--- | :--- | :--- |
| **Line Editing** | Left/Right cursor movement in place | Backspace and retype |
| **Command History** | Up/Down arrows scroll previous commands | History command list |
| **Screen Erase** | `ESC [ 2 J`, `ESC [ H` | Multiple blank newlines |
| **Color Coding** | ANSI 8/16 colors (Green ok, Yellow warn, Red err) | Plain monochrome text |
| **Cursor Control** | Full positioning and home/end | None |

---

## 5. Memory Management & Performance

- **Zero Dynamic Allocation**: The line editor, history buffer, and token parser use static buffers in BSS. No `malloc` or `new` calls occur during terminal interaction.
- **Line Buffer**: 128 bytes (sufficient for complex commands with multiple hexadecimal arguments).
- **History Buffer**: 8 command slots $\times$ 128 bytes $= 1024$ bytes RAM total.
- **Non-blocking Execution**: `terminal_task()` executes in microseconds per loop pass, yielding immediately when no serial bytes are pending.
