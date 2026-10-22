# UDOO KEY — Linux Troubleshooting & Diagnostic Guide

This guide provides step-by-step diagnostic procedures for resolving common issues encountered when running the UDOO KEY Workbench on Linux distributions.

---

## 1. Permission Denied on `/dev/ttyACM*`

### Symptom:
Connecting fails with `QSerialPort::PermissionError` or an error dialog stating:
```text
Permission denied: /dev/ttyACM0
```

### Cause:
By default, standard user accounts do not have write access to serial devices managed by the `dialout` group.

### Solution:
1. Ensure `dist/linux/99-udoo-key.rules` is copied to `/etc/udev/rules.d/`:
   ```bash
   sudo cp dist/linux/99-udoo-key.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   ```
2. Add your user account to the `dialout` and `plugdev` groups:
   ```bash
   sudo usermod -a -G dialout,plugdev $USER
   ```
3. **Log out of your Linux desktop session and log back in**, or run:
   ```bash
   newgrp dialout
   ```
4. Verify membership:
   ```bash
   groups
   # Should list 'dialout'
   ```

---

## 2. ModemManager Interfering with Serial Ports

### Symptom:
Upon plugging in the UDOO KEY RP2040, the device becomes busy for 5-15 seconds, drops initial packets, or commands like `HiZ>` fail to respond because AT commands were injected into the port.

### Cause:
The `ModemManager` daemon scans newly plugged serial devices attempting to detect cellular modems, probing them with `AT+...` strings.

### Solution:
The provided `99-udoo-key.rules` already contains `ENV{ID_MM_DEVICE_IGNORE}="1"`. If ModemManager is still interfering, filter it explicitly or stop ModemManager:
```bash
sudo systemctl stop ModemManager.service
sudo systemctl disable ModemManager.service
```

---

## 3. Missing Qt 6 Platform Plugins (`qt.qpa.plugin: Could not find the Qt platform plugin "xcb"`)

### Symptom:
Application exits immediately on startup with:
```text
qt.qpa.plugin: Could not find the platform plugin "xcb" in ""
This application failed to start because no Qt platform plugin could be initialized.
```

### Cause:
Missing XCB / Wayland runtime client libraries.

### Solution:
Install the required platform plugins:
* **Ubuntu/Debian**:
  ```bash
  sudo apt-get install -y libxcb-cursor0 libxcb-xinerama0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxkbcommon-x11-0
  ```
* **Fedora**:
  ```bash
  sudo dnf install -y libxcb xcb-util-cursor xcb-util-wm xcb-util-image xcb-util-keysyms xcb-util-renderutil
  ```

---

## 4. High-Rate Capture Dropping Samples over Serial

### Symptom:
Logic analyzer buffer capture completes on RP2040, but transmission over serial shows `FIFO overrun` or stuttering.

### Cause:
USB CDC buffer latency timer settings or high system latency.

### Solution:
1. Verify baud rate is set to maximum (`921600` or `1000000` / `2000000`).
2. Reduce USB latency timer (applicable for USB-serial bridges):
   ```bash
   echo 1 | sudo tee /sys/bus/usb-serial/devices/ttyUSB0/latency_timer
   ```
3. Use the desktop application's LOD (Level-of-Detail) decimator which automatically processes chunks asynchronously.

---

## 5. Firmware Flashing Issues (RP2040 BOOTSEL)

### Symptom:
Device does not mount as a USB storage drive (`RPI-RP2`) when holding BOOTSEL.

### Solution:
1. Unplug the USB cable.
2. Hold down the **BOOT** button on the UDOO KEY.
3. Plug in the USB cable, then release the **BOOT** button.
4. Check mount status:
   ```bash
   lsblk -f | grep RPI-RP2
   ```
5. If auto-mounting is disabled on headless servers, mount manually:
   ```bash
   sudo mkdir -p /mnt/pico
   sudo mount /dev/disk/by-label/RPI-RP2 /mnt/pico
   sudo cp firmware.uf2 /mnt/pico/
   sudo sync
   ```
