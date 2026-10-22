# UDOO KEY Clavis Probe Firmware Flashing Guide

This directory contains pre-compiled, verified production firmware for the **SECO UDOO KEY Dual-MCU Platform** (RP2040 + ESP32).

---

## 1. RP2040 Master Firmware (`udoo_key_rp2040_clavis_probe.uf2`)

The RP2040 runs the high-speed PIO logic capture engine, hardware protocol engines (I2C, SPI, UART, 1-Wire), JTAG/SWD debugger, and ST7735 1.44" 128×128 front-panel UI.

### Flashing via USB Bootloader (BOOTSEL)
1. Disconnect the UDOO KEY from USB power.
2. Press and hold the **`RP2040 BOOT`** button on the UDOO KEY.
3. While holding the button, plug the UDOO KEY USB-C cable into your PC.
4. Release the button once the mass-storage drive named **`RPI-RP2`** appears on your computer.
5. Drag and drop (or copy) **`udoo_key_rp2040_clavis_probe.uf2`** onto the `RPI-RP2` drive.
6. The drive will automatically unmount, and the RP2040 will reboot running the Clavis Probe firmware. The 1.44" ST7735 color display will initialize.

---

## 2. ESP32 Coprocessor Firmware (`udoo_key_esp32_coprocessor.bin`)

The ESP32 runs the wireless coprocessor firmware, providing Wi-Fi SoftAP telemetry, packet sniffer ingestion, and radio protocol bridging.

### Flashing via `esptool.py`
Connect the UDOO KEY to your PC and identify the ESP32 serial port (e.g., `COMx` on Windows or `/dev/ttyUSBx` on Linux):

```bash
# Install esptool if needed:
pip install esptool

# Flash ESP32 firmware at offset 0x10000:
esptool.py --chip esp32 --port COM5 --baud 921600 write_flash 0x10000 udoo_key_esp32_coprocessor.bin
```

---

## 3. Verification & First Run
1. Open the **UDOO KEY Clavis Workbench** desktop application (Windows or Linux).
2. Connect to the RP2040 CDC-ACM serial port at **115200 baud**.
3. Type `help` or `status` to verify communication with the instrument engine.
