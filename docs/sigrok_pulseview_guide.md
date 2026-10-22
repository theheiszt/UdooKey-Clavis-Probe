# UDOO KEY — Sigrok & PulseView Integration Guide

## 1. Overview & Architecture

The UDOO KEY measurement instrument firmware natively supports **Sigrok** and **PulseView** out of the box using the industry-standard **Openbench Logic Sniffer & SUMP (OLS)** protocol over USB CDC serial.

### Key Highlights
* **Zero Custom Drivers Required:** Works immediately with official, upstream release versions of **PulseView** and **sigrok-cli** on Windows, macOS, and Linux without compiling patched DLLs or installing proprietary drivers.
* **Hardware PIO Capture Engine:** Dedicated RP2040 PIO state machine samples the 8 contiguous probe pins at up to **100 MSPS** with sub-nanosecond jitter.
* **Synchronized Dual Display:** Capture progress and waveforms are displayed both inside the PC PulseView GUI and simultaneously on the onboard ST7735 1.44" color TFT screen.
* **Seamless CLI Auto-Switching:** Automatically transitions into binary SUMP mode upon detecting PulseView connection packets, and cleanly returns to the interactive Bus Pirate serial CLI when an Enter or ESC key is pressed in a terminal.

---

## 2. Hardware Probe Pinout (Header P1)

The logic analyzer channels map to the 8 contiguous RP2040 GPIOs on Header P1:

| PulseView Channel | RP2040 GPIO | UDOO KEY Pin | Header P1 Pin | Signal Function | Default Color |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **Probe 0** | `GP2` | Pin 4 | **P1, Pin 4** | Channel 0 (LSB) | Cyan |
| **Probe 1** | `GP3` | Pin 5 | **P1, Pin 5** | Channel 1 | Yellow |
| **Probe 2** | `GP4` | Pin 6 | **P1, Pin 6** | Channel 2 | Magenta |
| **Probe 3** | `GP5` | Pin 7 | **P1, Pin 7** | Channel 3 | Light Gray |
| **Probe 4** | `GP6` | Pin 8 | **P1, Pin 9** | Channel 4 | White |
| **Probe 5** | `GP7` | Pin 9 | **P1, Pin 10** | Channel 5 | Red |
| **Probe 6** | `GP8` | Pin 11 | **P1, Pin 11** | Channel 6 | Green |
| **Probe 7** | `GP9` | Pin 12 | **P1, Pin 12** | Channel 7 (MSB) | Blue |
| **GND** | `GND` | Ground | **P1, Pin 3 / 8 / 13** | Signal Ground Reference | Black |

> [!TIP]
> Always connect a signal ground wire between **Header P1 Pin 3, 8, or 13** and the ground of your target circuit to ensure signal integrity and avoid ground bounce during high-speed acquisitions.

---

## 3. Specifications & Capabilities

| Parameter | Specification | Details |
| :--- | :--- | :--- |
| **Channel Count** | 8 Digital Channels | Probes 0 through 7 (GP2..GP9) |
| **Maximum Sample Rate** | **100 MSPS** (100 MHz) | Hardware PIO state machine sampling |
| **Supported Sample Rates** | 2 kSPS to 100 MSPS | 100M, 50M, 20M, 10M, 5M, 2M, 1M, 500k, 200k, 100k, 50k, 20k, 10k, 5k, 2k |
| **Buffer Memory Depth** | **65,536 Samples** (64 KiB) | Dedicated circular SRAM ring buffer with DMA |
| **Voltage Levels** | 3.3V CMOS / LVTTL | 5V-tolerant when front-end protection is enabled |
| **Trigger Types** | Edge, Level, Pattern | Rising edge, Falling edge, High level, Low level, 8-bit pattern |
| **Pre-Trigger Ratio** | 0% to 100% | Configurable sample window before trigger inception |
| **Protocol Support** | Openbench Logic Sniffer (OLS) / SUMP | Device ID: `1ALS`, Protocol Version 2 |

---

## 4. Step-by-Step PulseView Connection Guide

### Step 1 — Connect the Hardware
1. Connect the UDOO KEY to your PC using a micro-USB cable.
2. Note the virtual COM port assigned by your operating system:
   * **Windows:** Device Manager &rarr; Ports (COM & LPT) &rarr; e.g., `COM4` or `COM5`.
   * **Linux:** `/dev/ttyACM0` or `/dev/ttyUSB0`.
   * **macOS:** `/dev/cu.usbmodem...`.

### Step 2 — Open PulseView & Connect
1. Launch **PulseView**.
2. Click the device selector button on the top toolbar (often displays *<No device>* or your last opened device).
3. In the device selection dialog:
   * **Choose the driver:** Select `Openbench Logic Sniffer & SUMP compatibles (ols)`.
   * **Choose the interface:** Select `Serial Port`.
   * **Serial port:** Select your COM port from the dropdown (e.g. `COM4` on Windows, or `/dev/ttyACM0` on Linux).
   * **Baud rate:** `115200` (USB CDC transfers run at full native USB speed regardless of baud setting).
4. Click **"Scan for devices using driver above"**.
5. PulseView queries the instrument via SUMP (`0x00`, `0x02`, `0x04`) and will immediately list:
   ```text
   UDOO KEY Logic Analyzer with 8 channels
   ```
6. Click **OK** to establish the connection.

```
       ┌─────────────────────────────────────────────────────────────┐
       │ Connect to Device                                           │
       ├─────────────────────────────────────────────────────────────┤
       │ Driver:    [ Openbench Logic Sniffer & SUMP compatibles (ols) ]
       │ Interface: [ Serial Port                                    ]
       │ Port:      [ COM4                                           ]
       │                                                             │
       │ [ Scan for devices using driver above ]                     │
       │                                                             │
       │ Found Devices:                                              │
       │ ● UDOO KEY Logic Analyzer with 8 channels                   │
       │                                                             │
       │                              [   OK   ]   [ Cancel ]        │
       └─────────────────────────────────────────────────────────────┘
```

---

## 5. Capturing Signals & Applying Protocol Decoders

### 5.1 Setting Acquisition Parameters
* In the PulseView top toolbar:
  * Select desired sample count (e.g., `10k`, `20k`, `50k`, `64k`).
  * Select desired sampling frequency (e.g., `1 MHz`, `10 MHz`, `50 MHz`, `100 MHz`).

### 5.2 Configuring Triggers
* Click on any channel label on the left (e.g., `Probe 0`):
  * **No trigger:** Free-running immediate capture.
  * **Rising Edge (/)**: Trips when signal transitions from LOW to HIGH.
  * **Falling Edge (\)**: Trips when signal transitions from HIGH to LOW.
  * **Level HIGH / LOW**: Trips on logic level.

### 5.3 Executing Capture
1. Click the **"Run"** button in PulseView.
2. The UDOO KEY on-screen ST7735 display will show **SIGROK** and **RUNNING**.
3. Once the trigger trips, the RP2040 transmits the chronologically aligned 64k buffer over USB.
4. Waveforms instantly render in PulseView and on the UDOO KEY TFT.

### 5.4 Using Hardware Protocol Decoders
PulseView includes over 100 open-source protocol decoders:
1. Click the **"Add Protocol Decoder"** icon on the top right.
2. Select your protocol (e.g., `I2C`, `SPI`, `UART`, `1-Wire`, `CAN`, `JTAG`).
3. Assign probe channels:
   * **I2C:** `SDA` &rarr; Probe 2 (GP4), `SCL` &rarr; Probe 3 (GP5).
   * **SPI:** `SCK` &rarr; Probe 0 (GP2), `MOSI` &rarr; Probe 1 (GP3), `MISO` &rarr; Probe 2 (GP4), `CS` &rarr; Probe 3 (GP5).
   * **UART:** `TX` &rarr; Probe 2 (GP4), `RX` &rarr; Probe 3 (GP5).
4. Decoded packet transactions (bytes, register addresses, ACK/NACK, parity) render below the waveform traces.

---

## 6. Command-Line Usage with `sigrok-cli`

For automated test benches or continuous capture scripts:

```bash
# 1. Scan for the UDOO KEY logic analyzer
sigrok-cli --driver ols:conn=COM4 --scan

# 2. Capture 10,000 samples at 10 MHz on 8 channels and display ASCII plot
sigrok-cli --driver ols:conn=COM4 --config samplerate=10M --samples 10000 -P ascii

# 3. Capture I2C traffic on channels 2 and 3 and decode to terminal
sigrok-cli --driver ols:conn=COM4 --config samplerate=5M --samples 20000 \
           --channels 2=sda,3=scl -P i2c:sda=sda:scl=scl
```

---

## 7. Seamless Terminal & CLI Interoperability

The firmware enables flexible switching between GUI analysis and interactive serial terminal use:
* **Automatic SUMP Detection:** When PulseView opens the port and transmits its handshake, the firmware activates SUMP mode without requiring manual terminal commands.
* **Manual Entry:** In any serial terminal (115200 baud), type `sump` to prime the listener:
  ```text
  UDOO-PIRATE> sump
  [SUMP Mode Active] Listening for Sigrok / PulseView / SUMP client on USB CDC.
  ```
* **Instant Exit back to CLI:** If you close PulseView and open a serial terminal, press **Enter** or **ESC**:
  ```text
  [SUMP Mode Inactive - Returned to CLI]
  UDOO-PIRATE> 
  ```
