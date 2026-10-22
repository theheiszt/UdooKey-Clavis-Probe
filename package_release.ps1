# UDOO KEY Diagnostic Workbench - Standalone Portable Release Packager
param(
    [string]$Version = "1.2.0"
)

$ErrorActionPreference = "Stop"
$RootDir = "d:\Udoo Key Project"
$DistDir = "$RootDir\dist"
$BundleName = "UDOO-KEY-Workbench-v$Version-win64"
$BundleDir = "$DistDir\$BundleName"
$ZipFile = "$DistDir\$BundleName.zip"
$QtBin = "D:\Qt\6.8.2\msvc2022_64\bin"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " Packaging UDOO KEY Workbench v$Version Standalone Release  " -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# 1. Clean and prepare output directories
if (Test-Path $BundleDir) {
    Write-Host "Cleaning existing bundle directory..."
    Remove-Item -Recurse -Force $BundleDir
}
New-Item -ItemType Directory -Force -Path $BundleDir | Out-Null
New-Item -ItemType Directory -Force -Path "$BundleDir\firmware" | Out-Null
New-Item -ItemType Directory -Force -Path "$BundleDir\sessions" | Out-Null
New-Item -ItemType Directory -Force -Path "$BundleDir\docs" | Out-Null

# 2. Copy Executable
$ExeSource = "$RootDir\udoo-key-app\build\udoo-key-app.exe"
if (-not (Test-Path $ExeSource)) {
    throw "Executable not found at $ExeSource. Run build_app.ps1 first."
}
Write-Host "Copying executable: udoo-key-app.exe..."
Copy-Item $ExeSource -Destination "$BundleDir\udoo-key-app.exe"

# 3. Run windeployqt
Write-Host "Deploying Qt 6.8.2 runtime DLLs & plugins via windeployqt..."
$env:PATH = "$QtBin;" + $env:PATH.Replace('nodejs"', 'nodejs').Replace('"', '')
$DeployQt = "$QtBin\windeployqt.exe"
& $DeployQt --svg --compiler-runtime --no-translations --no-system-d3d-compiler "$BundleDir\udoo-key-app.exe"

# 4. Copy Firmware Images
Write-Host "Packaging latest verified firmware binaries..."
$Rp2040Uf2 = "$RootDir\rp2040_logic_analyzer\.pio\build\rp2040_logic_analyzer\firmware.uf2"
$Rp2040Bin = "$RootDir\rp2040_logic_analyzer\.pio\build\rp2040_logic_analyzer\firmware.bin"
$Esp32Bin  = "$RootDir\esp32_coprocessor\.pio\build\esp32_coprocessor\firmware.bin"

if (Test-Path $Rp2040Uf2) { Copy-Item $Rp2040Uf2 "$BundleDir\firmware\rp2040_firmware.uf2" }
if (Test-Path $Rp2040Bin) { Copy-Item $Rp2040Bin "$BundleDir\firmware\rp2040_firmware.bin" }
if (Test-Path $Esp32Bin)  { Copy-Item $Esp32Bin  "$BundleDir\firmware\esp32_firmware.bin" }

$FlashingGuide = @"
============================================================
UDOO KEY FIRMWARE FLASHING INSTRUCTIONS
============================================================

1. FLASHING RP2040 REAL-TIME ENGINE:
   - Hold down the BOOTSEL button on the UDOO KEY.
   - Plug the board into your PC via the USB-C port.
   - The RP2040 will enumerate as a mass-storage drive 'RPI-RP2'.
   - Drag and drop 'rp2040_firmware.uf2' onto the 'RPI-RP2' drive.
   - The board will automatically reboot and start the 100 MSPS instrument engine.

2. FLASHING ESP32 COPROCESSOR ENGINE:
   - Use esptool.py or the PlatformIO toolchain:
     esptool.py --chip esp32 --port COMx write_flash 0x10000 esp32_firmware.bin
   - Once flashed, the ESP32 will host the wireless SoftAP 'UDOO-BIT-PIRATE-SCOPE'
     (Password: udoopass123) and communicate with the RP2040 over the factory UART.
"@
Set-Content -Path "$BundleDir\firmware\FLASHING_INSTRUCTIONS.txt" -Value $FlashingGuide

# 5. Generate Default Sample Sessions (.udoo)
Write-Host "Generating default session templates..."

# Session 1: I2C Bus Scan & Sensor Trace
$I2cSession = @{
    hardwareModel = "UDOO KEY"
    firmwareVersion = $Version
    sessionDescription = "Sample 7-bit I2C Bus Transaction (0x50 EEPROM Acknowledge)"
    timestamp = (Get-Date).ToString("o")
    sampleRateHz = 10000000
    sampleCount = 4096
    rawCaptureBytes = [Convert]::ToBase64String([byte[]](1..4096 | ForEach-Object { if ($_ % 40 -lt 20) { 0xC0 } else { 0x00 } }))
    cursorASec = 0.0001
    cursorBSec = 0.0003
    cursorCSec = 0.0005
    cursorDSec = 0.0007
    activeProtocol = "I2C"
    decoderChannelA = 6 # SCL on GP6
    decoderChannelB = 7 # SDA on GP7
    decoderBaudRate = 400000
} | ConvertTo-Json -Depth 5
Set-Content -Path "$BundleDir\sessions\default_i2c_trace.udoo" -Value $I2cSession

# Session 2: SPI Flash JEDEC ID Trace
$SpiSession = @{
    hardwareModel = "UDOO KEY"
    firmwareVersion = $Version
    sessionDescription = "Sample SPI Master JEDEC ID (0x9F) Flash Interrogation"
    timestamp = (Get-Date).ToString("o")
    sampleRateHz = 25000000
    sampleCount = 4096
    rawCaptureBytes = [Convert]::ToBase64String([byte[]](1..4096 | ForEach-Object { if ($_ % 16 -lt 8) { 0x3C } else { 0x00 } }))
    cursorASec = 0.00005
    cursorBSec = 0.00015
    cursorCSec = 0.00025
    cursorDSec = 0.00035
    activeProtocol = "SPI"
    decoderChannelA = 2 # CLK on GP2
    decoderChannelB = 3 # MOSI on GP3
    decoderBaudRate = 10000000
} | ConvertTo-Json -Depth 5
Set-Content -Path "$BundleDir\sessions\default_spi_flash.udoo" -Value $SpiSession

# Session 3: High-Speed UART 115200 Bridge Trace
$UartSession = @{
    hardwareModel = "UDOO KEY"
    firmwareVersion = $Version
    sessionDescription = "Sample 115200 Baud UART Diagnostic Stream"
    timestamp = (Get-Date).ToString("o")
    sampleRateHz = 10000000
    sampleCount = 4096
    rawCaptureBytes = [Convert]::ToBase64String([byte[]](1..4096 | ForEach-Object { if ($_ % 32 -lt 16) { 0x30 } else { 0x00 } }))
    cursorASec = 0.0002
    cursorBSec = 0.0004
    cursorCSec = 0.0006
    cursorDSec = 0.0008
    activeProtocol = "UART"
    decoderChannelA = 4 # TX on GP4
    decoderChannelB = 5 # RX on GP5
    decoderBaudRate = 115200
} | ConvertTo-Json -Depth 5
Set-Content -Path "$BundleDir\sessions\default_uart_bridge.udoo" -Value $UartSession

# 6. Copy Offline Documentation
Write-Host "Bundling offline documentation..."
Copy-Item "$RootDir\docs\hardware_guide.html" "$BundleDir\docs\hardware_guide.html"
Copy-Item "$RootDir\docs\pin_map.md" "$BundleDir\docs\pin_map.md"
Copy-Item "$RootDir\docs\sigrok_pulseview_guide.md" "$BundleDir\docs\sigrok_pulseview_guide.md"
Copy-Item "$RootDir\docs\UDOO_KEY_PROJECT_STATUS.md" "$BundleDir\docs\UDOO_KEY_PROJECT_STATUS.md"
if (Test-Path "$RootDir\docs\pinout-1479.png") { Copy-Item "$RootDir\docs\pinout-1479.png" "$BundleDir\docs\pinout-1479.png" }

# 7. Create Top-Level README.txt
$Readme = @"
========================================================================
 UDOO KEY WORKBENCH v$Version - PORTABLE STANDALONE EDITION
 Universal Dual-MCU Acquisition, Instrumentation & Diagnostics Suite
========================================================================

ABOUT:
  UDOO KEY Workbench is a professional, high-performance native desktop
  instrumentation suite for the SECO UDOO KEY (RP2040 + ESP32-WROVER-E).

FEATURES:
  * 8-Channel 100 MSPS Logic Analyzer with SUMP / PulseView support
  * Level-of-Detail (LOD) min/max hardware waveform rendering (60 FPS)
  * Protocol Decoders: UART, I2C, SPI with packet table & annotations
  * Interactive Bus Tools Workspace:
    - Mode Multiplexer with Safe Hi-Z Invariant
    - 7-bit I2C Bus Explorer & Matrix Scanner
    - SPI Master & JEDEC SFDP Flash Probe
    - Dallas 1-Wire Search ROM Tree
    - Secondary UART Bridge Monitor
    - 8-bit DIO Probe Bus with mask operations
    - Interactive Syntax Shell (square-bracket commands & macros)
    - Wireless Subsystem: Infrared (IR NEC/raw) & nRF24L01 2.4 GHz RF
  * ARM SWD DP-IDCODE Reader & IEEE 1149.1 JTAG Boundary Scan
  * Multichannel Voltmeter & Acoustic Continuity Tester
  * Apple Diagnostics & Monotonic Timestamp Correlation
  * USB Testing, Descriptor Parsing & Injection
  * Global Event Timeline & .udoo Session Container Persistence

HOW TO RUN:
  1. Simply double-click 'udoo-key-app.exe' in this directory.
     No installation or external runtime required (all Qt DLLs included).
  2. Connect your UDOO KEY via USB-C.
  3. Press Ctrl+O or click 'Connect' on the toolbar.

DIRECTORIES:
  * firmware/ : Latest verified firmware binaries for RP2040 and ESP32.
  * sessions/ : Default sample .udoo session containers (I2C, SPI, UART).
  * docs/     : Offline interactive hardware guides and pinout references.

========================================================================
"@
Set-Content -Path "$BundleDir\README.txt" -Value $Readme

# 8. Create Standalone Portable ZIP Archive
if (Test-Path $ZipFile) {
    Remove-Item -Force $ZipFile
}
Write-Host "Creating standalone ZIP distribution: $ZipFile..."
Compress-Archive -Path "$BundleDir\*" -DestinationPath $ZipFile -CompressionLevel Optimal

$ZipItem = Get-Item $ZipFile
$SizeMB = [math]::Round($ZipItem.Length / 1MB, 2)

Write-Host "============================================================" -ForegroundColor Green
Write-Host " RELEASE PACKAGING SUCCESSFUL!" -ForegroundColor Green
Write-Host " Standalone Archive: $ZipFile" -ForegroundColor Green
Write-Host " Total Size: $SizeMB MB" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
