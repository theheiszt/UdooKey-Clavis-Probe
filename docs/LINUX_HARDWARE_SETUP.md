# UDOO KEY — Linux Hardware Setup & Device Discovery

This document details how the Linux kernel discovers, enumerates, and configures the dual-MCU architecture of the UDOO KEY (Raspberry Pi RP2040 and Espressif ESP32-WROVER-E).

---

## 1. Board USB Connection Topology on Linux

The standard UDOO KEY routes its primary physical USB-C connector (CN1) directly to the RP2040:

```text
 Workstation Linux Host
        │
   [USB-C Cable]
        │
 UDOO KEY Connector CN1
        │
 RP2040 Native USB 1.1 Full-Speed
 VID: 0x2E8A, PID: 0x000A / 0x105A
 Device Node: /dev/ttyACM*
        │
 [High-Speed 1 Mbps Inter-MCU UART (GP0/GP1 ◄► IO19/IO22)]
        │
 ESP32-WROVER-E Coprocessor (Wi-Fi / BLE / Network Diagnostics)
```

### Enumeration Characteristics:

| Subsystem | Microcontroller | Kernel Driver | Device Node | VID:PID | Status | Function |
|---|---|---|---|---|---|---|
| **Main Instrument Engine** | RP2040 | `cdc_acm` | `/dev/ttyACM*` | `2e8a:000a` | **VERIFIED** | Logic Analyzer, SUMP, CLI, Bus Tools, Telemetry |
| **RP2040 Bootloader** | RP2040 | `usb-storage` | `/dev/sd*` | `2e8a:0003` | **VERIFIED** | Drag-and-drop UF2 firmware flashing |
| **Dual-CDC Instrument Protocol** | RP2040 | `cdc_acm` | `/dev/ttyACM*` | `2e8a:105a` | **VERIFIED** | Custom composite dual-stream firmware |

*(Note: The ESP32 communicates with the Linux workstation seamlessly over the high-speed 1 Mbps inter-MCU link routed through the RP2040 InstrumentProtocol packet demultiplexer.)*

---

## 2. Inspecting Connected Devices

### Check USB Enumeration:
```bash
lsusb | grep -i 2e8a
```
Expected output:
```text
Bus 001 Device 005: ID 2e8a:000a Raspberry Pi Pico
```

### Check Serial Ports:
```bash
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```
Expected permissions with udev rules active:
```text
crw-rw---- 1 root dialout 166, 0 Oct  5 21:00 /dev/ttyACM0
```

### Kernel Dmesg Events:
```bash
dmesg -w
```
When plugging in the RP2040, look for:
```text
cdc_acm 1-2:1.0: ttyACM0: USB ACM device
```

---

## 3. Persistent Stable Device Symlinks

When multiple USB serial devices are plugged into the system, device indices (`ttyACM0` vs `ttyACM3`) may change between reboots.

The provided `dist/linux/99-udoo-key.rules` can be customized to create a predictable persistent symlink:

```udev
# Symlink for RP2040 Main Instrument
SUBSYSTEM=="tty", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000a", SYMLINK+="udoo_key_instrument"
```

Once reloaded (`sudo udevadm control --reload-rules && sudo udevadm trigger`), you can always connect to `/dev/udoo_key_instrument`.

---

## 4. Modern Wayland & X11 Compatibility

The UDOO KEY Workbench Qt 6 GUI utilizes native hardware acceleration for waveform rendering:

* **Wayland (Ubuntu 22.04+, Fedora 39+)**: Supports native Wayland via `wayland-egl`.
* **X11 / XWayland**: Automatically falls back if Wayland is not present.
* To force X11 backend if display glitching occurs on older graphics drivers:
  ```bash
  QT_QPA_PLATFORM=xcb udoo-key-app
  ```
