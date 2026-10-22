# UDOO KEY — Master Pin Mapping Specification

**Document Version:** 1.0.0  
**Date:** 2026-10-03  
**Status:** Canonical Hardware Reference  
**Board:** SECO UDOO KEY (BD REV O / CSD81REVB)  

---

## 1. RP2040 Pin Map (GP0 – GP29)

| Pin | Physical Header | Default Function | Apple Diagnostics Role | Direction | Type | Constraints & Protection |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **GP0** | Onboard Trace | UART0 TX | Inter-MCU UART0 TX | Output | 3.3V CMOS | **LOCKED.** Factory PCB copper link to ESP32 IO22. |
| **GP1** | Onboard Trace | UART0 RX | Inter-MCU UART0 RX | Input | 3.3V CMOS | **LOCKED.** Factory PCB copper link from ESP32 IO19. |
| **GP2** | Header P1 Pin 4 | Logic LA_CH0 | `PIN_APPLE_CC1` | In / Out | 3.3V Tolerant | Contiguous PIO Bus Bit 0. USB Type-C CC1 / Lightning ID0. |
| **GP3** | Header P1 Pin 5 | Logic LA_CH1 | `PIN_APPLE_CC2` | In / Out | 3.3V Tolerant | Contiguous PIO Bus Bit 1. USB Type-C CC2 / Lightning ID1. |
| **GP4** | Header P1 Pin 6 | Logic LA_CH2 | `PIN_APPLE_VBUS_EN` | In / Out | 3.3V Tolerant | Contiguous PIO Bus Bit 2. VBUS Enable / Power switch. |
| **GP5** | Header P1 Pin 7 | Logic LA_CH3 | `PIN_APPLE_ACC_SDA` | In / Out | 3.3V Tolerant | Contiguous PIO Bus Bit 3. Accessory I2C SDA / Single-Wire bus. |
| **GP6** | Header P1 Pin 9 | Logic LA_CH4 | `PIN_APPLE_ACC_SCL` | In / Out | 3.3V Tolerant | Contiguous PIO Bus Bit 4. Accessory I2C SCL. |
| **GP7** | Header P1 Pin 10 | Logic LA_CH5 | `PIN_APPLE_ACC_RX` | In / Out | 3.3V Tolerant | Contiguous PIO Bus Bit 5. Accessory UART RX / SWD DIO. |
| **GP8** | Header P1 Pin 11 | Logic LA_CH6 | `PIN_APPLE_ACC_TX` | In / Out | 3.3V Tolerant | Contiguous PIO Bus Bit 6. Accessory UART TX / SWD CLK. |
| **GP9** | Header P1 Pin 12 | Logic LA_CH7 | `PIN_APPLE_SYNC_STROBE`| Output | 3.3V CMOS | Strobe pulse out to sync external protocol analyzer. |
| **GP10** | Header P1 Pin 14 | `PIN_EXT_TRIG` | External Trigger In | Input | 3.3V CMOS | Dedicated hardware trigger input comparator. |
| **GP11** | Onboard Trace | ST7735 TFT RST | Display UI Reset | Output | 3.3V CMOS | **LOCKED.** Active LOW display reset. |
| **GP12** | Onboard Trace | ST7735 TFT D/C | Display UI Data/Cmd | Output | 3.3V CMOS | **LOCKED.** Low=Command, High=Data. |
| **GP13** | Onboard Trace | ST7735 TFT CS | Display UI Chip Select | Output | 3.3V CMOS | **LOCKED.** Dedicated display chip select. |
| **GP14** | Header / Circuit | Front-End Prescaler | Prescaler Control | Output | 3.3V CMOS | Controls external 74HC74 1:1, 1:2, 1:4 divider. |
| **GP15** | Pushbutton UP | Keypad UP | UI Navigation UP | Input | Pull-Up | **LOCKED.** Active LOW to GND. |
| **GP16** | Pushbutton DOWN | Keypad DOWN | UI Navigation DOWN | Input | Pull-Up | **LOCKED.** Active LOW to GND. |
| **GP17** | Pushbutton LEFT | Keypad LEFT | UI Navigation LEFT | Input | Pull-Up | **LOCKED.** Active LOW to GND. |
| **GP18** | Onboard Trace | ST7735 SPI0 SCK | Display UI SCK | Output | 3.3V CMOS | **LOCKED.** Hardware SPI0 Clock. |
| **GP19** | Onboard Trace | ST7735 SPI0 MOSI | Display UI MOSI | Output | 3.3V CMOS | **LOCKED.** Hardware SPI0 TX. |
| **GP20** | Header P1 Pin 15 | Buzzer / Tone | Audio Continuity Alert | Output | PWM Slice 2A | Piezo resonant buzzer drive (2.7 kHz). |
| **GP21** | Pushbutton RIGHT | Keypad RIGHT | UI Navigation RIGHT | Input | Pull-Up | **LOCKED.** Active LOW to GND. |
| **GP22** | Pushbutton SELECT | Keypad SELECT | UI Run / Select | Input | Pull-Up | **LOCKED.** Active LOW to GND. |
| **GP23** | Internal Circuit | SMPS Power-Save | SMPS PS Control | Output | Power Rail | **LOCKED.** Internal buck regulator low-noise mode. |
| **GP24** | Internal Circuit | USB VBUS Detect | Host USB Sense | Input | Power Sense | **LOCKED.** Host USB connection detect. |
| **GP25** | Onboard Green LED | Activity LED | Trigger Strobe LED | Output | LED Drive | Active HIGH green indicator LED. |
| **GP26** | Header P2 Pin 11 | ADC0 (0..3.3V) | `PIN_APPLE_VBUS_SENSE` | Analog In | 12-Bit SAR | Scaled VBUS voltage monitoring (0..20V via divider). |
| **GP27** | Header P2 Pin 12 | ADC1 (0..3.3V) | `PIN_APPLE_SHUNT_CURRENT`| Analog In| 12-Bit SAR | Current shunt monitor (Charge / Accessory current). |
| **GP28** | Header P2 Pin 14 | ADC2 (0..3.3V) | `PIN_APPLE_ACC_VOLTAGE` | Analog In | 12-Bit SAR | Auxiliary accessory rail monitor. |
| **GP29** | Internal Circuit | VSYS Sense (ADC3) | VSYS Voltage Sense | Analog In | 12-Bit SAR | **LOCKED.** Scaled 1/3 VSYS internal rail monitor. |

---

## 2. ESP32 Pin Map

| Pin | Physical Connection | Function | Apple Diagnostics Role | Direction | Constraints |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **GPIO22** | Hardwired Trace | UART RX (◄ RP2040 GP0) | Inter-MCU Transport RX | Input | **LOCKED.** 1,000,000 baud 8N1 link. |
| **GPIO19** | Hardwired Trace | UART TX (➔ RP2040 GP1) | Inter-MCU Transport TX | Output | **LOCKED.** 1,000,000 baud 8N1 link. |
| **GPIO23** | Hardwired Trace | RP2040 RUN (Reset) | RP2040 Hardware Watchdog | Output | **LOCKED.** Active LOW reset control. |
| **GPIO2** | Onboard Trace | SWDIO to RP2040 | RP2040 On-Chip Debugger | In / Out | High-Z during normal instrument operation. |
| **GPIO4** | Onboard Trace | SWCLK to RP2040 | RP2040 On-Chip Debugger | Output | High-Z during normal instrument operation. |
| **GPIO5** | Onboard Trace | SWD Buffer Output Enable| Buffer Control | Output | Active LOW enable. |
| **GPIO21** | UEXT Pin 6 / Header | I2C SDA | Bit Pirate I2C Bus | In / Out | Non-conflicting open-drain bus. |
| **GPIO26** | Header | I2C SCL | Bit Pirate I2C Bus | Output | Non-conflicting open-drain clock. |
| **GPIO25** | Header | PWM Generator Out | Test Signal Generator | Output | Hardware LEDC PWM stimulus. |
| **GPIO0** | User Button | Boot / Button | Bootloader / Diagnostics | Input | Active LOW. |
| **GPIO32** | Onboard LED 1 | Status Indicator | Radio Activity LED | Output | Active HIGH. |
| **GPIO33** | Onboard LED 2 | Status Indicator | Sync Activity LED | Output | Active HIGH. |
