#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <stdint.h>

/*
 * ==============================================================================
 * UDOO KEY Dual-MCU Diagnostic Instrument — Master Hardware Configuration
 * ==============================================================================
 *
 * This file is the CANONICAL SINGLE SOURCE OF TRUTH for all physical pinouts,
 * MCU-to-MCU connections, peripheral allocations, electrical definitions,
 * buffer sizes, and transport settings.
 *
 * Applicable Hardware:
 *   - Board: SECO UDOO KEY (Model: BD REV O / CSD81REVB)
 *   - Real-Time Acquisition Engine: Raspberry Pi RP2040 (Dual-Core ARM Cortex-M0+ @ 133 MHz)
 *   - Communications & UI Engine: Espressif ESP32-WROVER-E (Dual-Core Xtensa LX6 @ 240 MHz)
 *
 * Hardware Authorities:
 *   1. UDOO KEY Schematic (Authority 1)
 *   2. Official SECO UDOO KEY Manual Rev 1.6 (Authority 2)
 *   3. UDOO KEY Arduino REFERENCE.md (Authority 3)
 *
 * Rules:
 *   - Never scatter raw GPIO numbers in source files. Always use these macros.
 *   - Never confuse namespaces: RP2040 GPx != ESP32 GPIOx.
 * ==============================================================================
 */

// ─── Instrument System Constants ─────────────────────────────────────────────
#define INSTRUMENT_NAME              "UDOO KEY Logic Scope & Bus Pirate"
#define INSTRUMENT_VERSION_MAJOR     2
#define INSTRUMENT_VERSION_MINOR     1
#define INSTRUMENT_BUILD_DATE        "2026-10-03"

// Capture Buffer & Circular DMA Parameters (64 KiB = 65,536 Samples)
#define CAPTURE_BUFFER_SIZE          65536
#define CAPTURE_PRE_TRIGGER_DEFAULT_PCT 70  // 70% Pre-Trigger / 30% Post-Trigger
#define CAPTURE_SAMPLE_RATE_DEFAULT  10000000 // 10 MSPS Default
#define CAPTURE_SAMPLE_RATE_MAX      100000000 // 100 MSPS Max PIO Acquisition

// Instrument Operating Profiles
#define INSTRUMENT_PROFILE_GENERIC          0 // Standard Logic Analyzer & Protocol Tool
#define INSTRUMENT_PROFILE_LAB              1 // Electronics Diagnostic Lab Mode
#define INSTRUMENT_PROFILE_APPLE_DIAGNOSTICS 1 // Compatibility alias

// Hardware External Analyzer Synchronization
#define PIN_SYNC_OUT                 9  // GP9: Pulse strobe output to sync external analyzers (when not CH7)
#define PIN_SYNC_OUT_ALT             10 // GP10: Alternative sync strobe output (repurposing EXT_TRIG)

// Inter-MCU Serial Transport
#define INTERMCU_UART_BAUDRATE       1000000 // 1,000,000 baud 8N1 Native Factory Link

// ─── RP2040 Master Pin Map (GP0 – GP29) ──────────────────────────────────────

// 1. Factory Inter-MCU UART Link (Hardwired PCB copper traces)
#define PIN_INTERMCU_TX              0   // GP0: RP2040 UART0 TX ──► ESP32 IO22 (RX)
#define PIN_INTERMCU_RX              1   // GP1: RP2040 UART0 RX ◄── ESP32 IO19 (TX)

// 2. 8-Channel Logic Analyzer Probe Bus (Contiguous GP2..GP9 for direct PIO sampling)
#define PIN_LA_CH0                   2   // GP2: Logic Analyzer Channel 0 (PIO Bus Bit 0)
#define PIN_LA_CH1                   3   // GP3: Logic Analyzer Channel 1 (PIO Bus Bit 1)
#define PIN_LA_CH2                   4   // GP4: Logic Analyzer Channel 2 (PIO Bus Bit 2)
#define PIN_LA_CH3                   5   // GP5: Logic Analyzer Channel 3 (PIO Bus Bit 3)
#define PIN_LA_CH4                   6   // GP6: Logic Analyzer Channel 4 (PIO Bus Bit 4)
#define PIN_LA_CH5                   7   // GP7: Logic Analyzer Channel 5 (PIO Bus Bit 5)
#define PIN_LA_CH6                   8   // GP8: Logic Analyzer Channel 6 (PIO Bus Bit 6)
#define PIN_LA_CH7                   9   // GP9: Logic Analyzer Channel 7 (PIO Bus Bit 7)
#define PIN_LA_BASE                  2   // First GPIO pin of contiguous 8-bit bus
#define PIN_LA_COUNT                 8   // 8 digital channels

// 3. Hardware Trigger & Front-End Prescaler Controls
#define PIN_EXT_TRIG                 10  // GP10: Dedicated External Hardware Trigger In
#define PIN_FRONTEND_PRESCALER       14  // GP14: External 74HC74 Prescaler Select (1:1, 1:2, 1:4)

// 4. ST7735 1.44" SPI TFT Display Subsystem (Hardware SPI0)
#define PIN_TFT_RST                  11  // GP11: TFT Hardware Reset (Active LOW)
#define PIN_TFT_DC                   12  // GP12: TFT Data / Command (0=Cmd, 1=Data)
#define PIN_TFT_CS                   13  // GP13: TFT Chip Select (Dedicated to Display)
#define PIN_TFT_SCK                  18  // GP18: RP2040 SPI0 SCK (Display Clock)
#define PIN_TFT_MOSI                 19  // GP19: RP2040 SPI0 TX / MOSI (Display Data)
#define PIN_TFT_MISO                 20  // GP20: RP2040 SPI0 RX / MISO

// 5. Five-Button Tactile Navigation Keypad (Active LOW to GND, Internal Pull-Up)
#define PIN_BTN_UP                   15  // GP15: Button UP (Navigate Up / Menu Prev)
#define PIN_BTN_DOWN                 16  // GP16: Button DOWN (Navigate Down / Menu Next)
#define PIN_BTN_LEFT                 17  // GP17: Button LEFT (Pan Left / Dec Timebase)
#define PIN_BTN_RIGHT                21  // GP21: Button RIGHT (Pan Right / Inc Timebase)
#define PIN_BTN_SELECT               22  // GP22: Button SELECT (Enter / Run / Stop)

// 6. RP2040 Internal & Reserved Hardware Signals
#define PIN_INTERNAL_SMPS_PS         23  // GP23: Internal Power-Supply SMPS Power-Save (DO NOT REPURPOSE)
#define PIN_INTERNAL_VBUS_SENSE      24  // GP24: Internal USB VBUS Detect (DO NOT REPURPOSE)
#define PIN_ONBOARD_LED              25  // GP25: Onboard Green LED (Activity Indicator)
#define PIN_INTERNAL_VSYS_SENSE      29  // GP29: Internal VSYS Voltage Monitor ADC3 (DO NOT REPURPOSE)

// 7. 3-Channel Precision Analog Voltmeter & Multifunction ADC Subsystem (RP2040 12-Bit SAR ADC)
#define PIN_ADC0                     26  // GP26: ADC0 (Analog Channel 0, Header P2 Pin 11 / Pico Pin 31, 0-3.3V)
#define PIN_ADC1                     27  // GP27: ADC1 (Analog Channel 1, Header P2 Pin 12 / Pico Pin 32, 0-3.3V)
#define PIN_ADC2                     28  // GP28: ADC2 (Analog Channel 2, Header P2 Pin 14 / Pico Pin 34, 0-3.3V)
#define PIN_BUZZER                   20  // GP20: Dedicated Audio / Continuity Buzzer (Header P1 Pin 15, PWM Slice 2A)
#define ADC_CHANNEL_COUNT            3
#define ADC_VREF_VOLTS               3.3f
#define ADC_MAX_RAW_VALUE            4095.0f
#define ADC_WAVEFORM_MAX_SAMPLES     2048 // 2048 16-bit samples = 4 KiB RAM (zero conflict with 64 KiB LA buffer)
#define CONTINUITY_DEFAULT_THRESH_OHMS 25.0f // Standard bench continuity threshold (<25 ohms = beep)
#define CONTINUITY_DEFAULT_HYST_OHMS   10.0f // Hysteresis release threshold (>35 ohms = silence)
#define CONTINUITY_R_REF_OHMS        1000.0f // 1 kOhm precision pull-up reference for resistance bridge
#define CONTINUITY_BUZZER_FREQ_HZ    2700    // Resonant piezo frequency (2.7 kHz)

// 8. Electronics Laboratory Diagnostic Dedicated Pin Mappings (Hardware Aliases)
#define PIN_LAB_PROBE_CH0            PIN_LA_CH0           // GP2: Probe Channel 0
#define PIN_LAB_PROBE_CH1            PIN_LA_CH1           // GP3: Probe Channel 1
#define PIN_LAB_PROBE_CH2            PIN_LA_CH2           // GP4: Probe Channel 2
#define PIN_LAB_PROBE_CH3            PIN_LA_CH3           // GP5: Probe Channel 3
#define PIN_LAB_PROBE_CH4            PIN_LA_CH4           // GP6: Probe Channel 4
#define PIN_LAB_PROBE_CH5            PIN_LA_CH5           // GP7: Probe Channel 5
#define PIN_LAB_PROBE_CH6            PIN_LA_CH6           // GP8: Probe Channel 6
#define PIN_LAB_PROBE_CH7            PIN_LA_CH7           // GP9: Probe Channel 7
#define PIN_LAB_SYNC_STROBE          PIN_SYNC_OUT         // GP9: External Analyzer Sync Strobe
#define PIN_LAB_ANALOG_ADC0          PIN_ADC0             // GP26: ADC Channel 0 Sense
#define PIN_LAB_ANALOG_ADC1          PIN_ADC1             // GP27: ADC Channel 1 Sense
#define PIN_LAB_ANALOG_ADC2          PIN_ADC2             // GP28: ADC Channel 2 Sense

// RP2040 Hardwired Protected Pin Mask (Pins that can NEVER be dynamically reassigned)
// Bits: 0,1 (UART), 11,12,13,18,19,20 (Display), 15,16,17,21,22 (Buttons), 23,24,29 (System)
#define RP2040_LOCKED_PINS_MASK      ((1UL<<0)|(1UL<<1)|(1UL<<11)|(1UL<<12)|(1UL<<13)|(1UL<<15)|(1UL<<16)|(1UL<<17)|(1UL<<18)|(1UL<<19)|(1UL<<20)|(1UL<<21)|(1UL<<22)|(1UL<<23)|(1UL<<24)|(1UL<<29))

// ─── ESP32 Master Pin Map ───────────────────────────────────────────────────

// 1. Factory Inter-MCU UART Link (Hardwired to RP2040 GP0/GP1)
#define ESP32_PIN_UART_RX            22  // GPIO22: ESP32 UART RX ◄── RP2040 GP0 (TX)
#define ESP32_PIN_UART_TX            19  // GPIO19: ESP32 UART TX ──► RP2040 GP1 (RX)

// 2. RP2040 System Reset Control (Hardwired on PCB)
#define ESP32_PIN_RPI_RESET          23  // GPIO23: Active LOW RP2040 Hardware Reset Control

// 3. Inter-MCU SWD Debug Link (Hardwired on PCB)
#define ESP32_PIN_SWDIO              2   // GPIO2:  ESP32 to RP2040 SWDIO
#define ESP32_PIN_SWCLK              4   // GPIO4:  ESP32 to RP2040 SWCLK
#define ESP32_PIN_SWD_OE             5   // GPIO5:  External SWD Buffer Enable

// 4. Onboard Peripherals on ESP32 Side
#define ESP32_PIN_USER_BTN           0   // GPIO0:  Boot / User Button
#define ESP32_PIN_LED_IO32           32  // GPIO32: Onboard LED 1
#define ESP32_PIN_LED_IO33           33  // GPIO33: Onboard LED 2

// 5. ESP32 UEXT Connector (P3) Signals
#define ESP32_UEXT_TXD               17  // GPIO17: UEXT Pin 3
#define ESP32_UEXT_RXD               16  // GPIO16: UEXT Pin 4
#define ESP32_UEXT_SCL               22  // Shared or jumpered SCL
#define ESP32_UEXT_SDA               21  // GPIO21: UEXT Pin 6
#define ESP32_UEXT_MISO              35  // GPIO35: UEXT Pin 7 (Input only)
#define ESP32_UEXT_MOSI              12  // GPIO12: UEXT Pin 8
#define ESP32_UEXT_SCK               14  // GPIO14: UEXT Pin 9
#define ESP32_UEXT_SSEL              15  // GPIO15: UEXT Pin 10

#endif // HARDWARE_CONFIG_H
