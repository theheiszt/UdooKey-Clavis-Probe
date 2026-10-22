# UDOO KEY Workbench — Linux Installation Guide

This guide covers building, installing, and running the native C++20 / Qt 6 UDOO KEY Workbench desktop application on Linux distributions including **Ubuntu 24.04 / 22.04 LTS**, **Debian 12 (Bookworm) / 13 (Trixie)**, **Fedora 39+**, and **Arch Linux**.

---

## 1. System Requirements

* **Architecture**: x86_64, aarch64 (Raspberry Pi 4/5, Armbian)
* **Compiler**: GCC 12+ or Clang 15+ (with full C++20 support)
* **Build System**: CMake 3.20+, Ninja
* **GUI Toolkit**: Qt 6.5 or newer (Qt Core, Gui, Widgets, SerialPort, Concurrent, Svg)
* **USB & Kernel**: Linux kernel 5.15+ with `cdc_acm`, `cp210x`, and `ch341` modules enabled (standard in all default kernels).

---

## 2. Installing Prerequisites

### Ubuntu 24.04 / 22.04 LTS & Debian 12/13
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    qt6-base-dev \
    qt6-serialport-dev \
    libqt6svg6-dev \
    libgl1-mesa-dev
```

### Fedora 39 / 40 / 41
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    qt6-qtbase-devel \
    qt6-qtserialport-devel \
    qt6-qtsvg-devel \
    mesa-libGL-devel
```

### Arch Linux / Manjaro
```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    ninja \
    qt6-base \
    qt6-serialport \
    qt6-svg
```

---

## 3. Configuring udev Rules & User Permissions (Crucial)

By default, Linux restricts access to serial communication devices (`/dev/ttyACM*`, `/dev/ttyUSB*`) to `root` and members of the `dialout` or `uaccess` groups.

### Step 1: Install udev Rules
From the project root:
```bash
sudo cp dist/linux/99-udoo-key.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### Step 2: Add User to Groups
```bash
sudo usermod -a -G dialout,plugdev $USER
```
> [!IMPORTANT]
> You **must log out and log back in** (or run `newgrp dialout`) for the new group membership to take effect in your user session.

---

## 4. Building from Source

### Automated Build Script
```bash
cd dist/linux
chmod +x build_linux.sh
./build_linux.sh Release
```

### Manual CMake & Ninja Compilation
```bash
cd udoo-key-app
mkdir -p build-linux && cd build-linux
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
ninja -j$(nproc)
```

### Running CTest Verification
Verify all 15 automated test suites pass:
```bash
ctest --output-on-failure
```

---

## 5. System-Wide Installation

From the build directory:
```bash
sudo ninja install
sudo update-desktop-database
sudo update-mime-database /usr/local/share/mime
```

This installs:
* Binary: `/usr/local/bin/udoo-key-app`
* Desktop Launcher: `/usr/local/share/applications/udoo-key.desktop`
* Icon: `/usr/local/share/icons/hicolor/scalable/apps/udoo-key.svg`
* MIME Association: `/usr/local/share/mime/packages/udoo-key-mime.xml`
* Hardware Rules: `/lib/udev/rules.d/99-udoo-key.rules`

---

## 6. Packaging

### Debian / Ubuntu (.deb) Package
To generate an installable `.deb` package:
```bash
cd dist/linux
chmod +x package_deb.sh
./package_deb.sh
```
Install the resulting package:
```bash
sudo dpkg -i dist/linux/output/udoo-key-workbench_*.deb
# Fix any missing system dependencies if prompted:
sudo apt-get install -f
```

### Linux x86_64 AppImage
To generate a standalone AppImage for Linux x86_64 distributions:
```bash
cd dist/linux
chmod +x package_appimage.sh
./package_appimage.sh
```
Target distributions tested:
- **Ubuntu 24.04 LTS (Noble Numbat)**: PASS (tested in clean desktop container/install)
- **Ubuntu 22.04 LTS (Jammy Jellyfish)**: PASS
- **Debian 12 (Bookworm)**: PASS

Run the AppImage:
```bash
chmod +x dist/linux/output/UDOO_KEY_Workbench-x86_64.AppImage
./dist/linux/output/UDOO_KEY_Workbench-x86_64.AppImage
```

---

## 7. Running the Application

### Via Desktop Application Menu
Launch **"UDOO KEY Workbench"** from your system application menu under **Development** or **Electronics**.

### Via Terminal
```bash
# Standard hardware connection mode
udoo-key-app

# Direct simulation mode (no hardware required)
udoo-key-app --mock
```
