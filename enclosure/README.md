# UDOO KEY Handheld Logic Analyzer Enclosure
## Expanded Daughterboard Bay & 5-Button Keypad Edition

A precision, parametric 3D-printable enclosure and complete wiring integration guide custom-engineered for the **UDOO KEY (RP2040 + ESP32)** running the high-speed Logic Scope & Bus Pirate firmware with a **single ST7735 1.44" SPI TFT display** and **5-button direct tactile navigation keypad**.

---

## 1. Mechanical Specifications

| Parameter | Specification | Notes |
| :--- | :---: | :--- |
| **Enclosure Outer Dimensions**| $72.6 \times 137.6 \times 27.3\text{ mm}$ | Expanded handheld form factor with open daughterboard bay |
| **Internal Cavity Dimensions** | $67.8 \times 132.8 \times 22.5\text{ mm}$ | Houses UDOO KEY PCB + $+25\text{ mm}$ right-hand bay |
| **UDOO KEY PCB Fit** | $130.0 \times 40.0 \times 10.9\text{ mm}$ | Anchored on left wall so USB-C remains flush with opening |
| **Daughterboard Bay (+X)** | $+25.0\text{ mm}$ internal width | Space for shields/PCBs mounted on RP2040 pinout (P1/P2) |
| **Vertical Headroom (+Z)** | $+4.0\text{ mm}$ extra Z clearance ($22.5\text{ mm}$ internal height) | Ample clearance for tall stacking headers and components |
| **Mounting Standoffs** | 6x raised posts ($H = 5.0\text{ mm}$, $OD = 6.0\text{ mm}$) | Sized for M2 screws ($ID = 2.0\text{ mm}$) with root fillets |
| **Display Viewport** | $27.0 \times 27.0\text{ mm}$ with $45^\circ$ viewing chamfer | Centered over UDOO KEY board |
| **Navigation Keypad** | 5x Direct $6\times 6\text{ mm}$ tactile switches (No plungers) | Molded snap-fit cradles on lid ceiling |
| **Finger Contouring** | $9.6\text{ mm}$ Concave finger dishes with $3.9\text{ mm}$ stem holes | Smooth depressions make the $1.1\text{ mm}$ switch stems comfortable |
| **Side Connector Port** | $13.5 \times 7.0\text{ mm}$ USB Type-C slot on left wall ($Y = 60.0\text{ mm}$) | Accommodates standard USB-C cable connector overmolds |
| **Front Probe Port** | $18.0 \times 7.0\text{ mm}$ slot aligned with Pico blade | Direct pass-through for GP2..GP9 logic probe harness |
| **Case Fasteners** | 4x M3 Socket Head Cap Screws ($L = 20\text{ mm}$) | Corner pillars with bottom counterbores & top brass inserts |
| **Case Sealing** | Perimeter stepped tongue-and-groove lip | Blocks dust and light bleed while ensuring rigid alignment |

---

## 2. Definitive Hardware Wiring Guide

### 2.1 ST7735 1.44" SPI TFT Display (128x128)

| Display Pin | Function | UDOO KEY Header Pin | RP2040 Signal | Wire Color | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **VCC** | Power (+3.3V) | **Pin 36** (Right) | `3V3_OUT` (+3.3V) | **RED** | Connect to clean 3.3V supply |
| **GND** | Ground (0V) | **Pin 38** (Right) | `GND` | **BLACK** | Common system ground |
| **CS** | Chip Select | **Pin 17** (Left) | **GP13** | **ORANGE** | Active LOW hardware select |
| **RESET** | Hardware Reset | **Pin 15** (Left) | **GP11** | **WHITE** | Active LOW reset pulse on boot |
| **A0 / DC** | Data / Command | **Pin 16** (Left) | **GP12** | **YELLOW** | LOW = Command, HIGH = Data |
| **SDA** | SPI MOSI | **Pin 25** (Right) | **GP19** (`SPI0_TX`)| **GREEN** | RP2040 MOSI output |
| **SCK** | SPI Clock | **Pin 24** (Right) | **GP18** (`SPI0_SCK`)| **BLUE** | RP2040 SPI Clock (up to 31.25 MHz) |
| **LED** | Backlight | **Pin 36** (Right) | `3V3_OUT` (+3.3V) | **MAGENTA** | Tie to 3.3V for full brightness |

### 2.2 5-Button Tactile Navigation Keypad (Direct 6x6 Switches)

| Button | Instrument Function | Menu Navigation Role | UDOO KEY Header Pin | RP2040 GPIO | Signal Wire |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **UP** | Navigate Up / Menu Prev | Cursor Up / Setting Inc | **Pin 20** (Left) | **GP15** | **CRIMSON** |
| **DOWN** | Navigate Down / Menu Next | Cursor Down / Setting Dec | **Pin 21** (Right) | **GP16** | **ORANGE** |
| **LEFT** | Pan Left / Dec Timebase | Back / Return to Parent Menu | **Pin 22** (Right) | **GP17** | **YELLOW** |
| **RIGHT**| Pan Right / Inc Timebase | Select / Enter Submenu | **Pin 27** (Right) | **GP21** | **ROYAL BLUE**|
| **SELECT**| Enter / Run / Stop | Execute / Toggle / Enter | **Pin 29** (Right) | **GP22** | **PURPLE** |
| **GND** | Common Ground Bus | Common Ground Bus | **Pin 23** (Right) | `GND` | **BLACK** |

* **Zero External Pull-Ups**: Configured in firmware via `pinMode(pin, INPUT_PULLUP)` utilizing the RP2040's internal ~50kΩ pull-up resistors.
* **Active Level**: Active LOW when pressed to GND.
* **Ergonomic D-Pad**: Pressing `RIGHT` or `SELECT` enters the selected submenu, allowing seamless one-handed thumb navigation. Pressing `LEFT` navigates back.

### 2.3 8-Channel Logic Analyzer Probe Bus

* **Channels CH0..CH7**: RP2040 **GP2..GP9** (Header Pins 4, 5, 6, 7, 9, 10, 11, 12).
* **Hardware Trigger**: RP2040 **GP10** (Header Pin 14).
* **Ground Reference**: Pins 3, 8, 13 (`GND`).

---

## 3. Hardware Mapping & Standoff Coordinates

All standoff coordinates are referenced from the **PCB bottom-left corner datum** ($X=0\dots 130\text{ mm}$, $Y=0\dots 40\text{ mm}$):

```
       Y=40mm ┌─────────────────────────[USB-C CN1]─────────────────────────────┐
              │                                                                 │ (Hole 5)
              │                     UDOO KEY MAIN BOARD                         │  X=120, Y=37
              │                     (ESP32 + UEXT P3)                           │
       Y=30.5 │     ┌───────────────────┬───────────────────────────────────────┤
              │     │ (Hole 1)          │ (Hole 3)                              │
              │     │  X=2, Y=25.7      │  X=49, Y=25.7                         │
              │     │   RP2040 PICO     │                                       │
              │     │   BLADE (GP2..GP9)│                                       │
              │     │ (Hole 2)          │ (Hole 4)                              │
              │     │  X=2, Y=14.3      │  X=49, Y=14.3                         │
        Y=9.5 │     └───────────────────┴───────────────────────────────────────┤
              │                                                                 │ (Hole 6)
              │                                                                 │  X=120, Y=3
        Y=0mm └─────────────────────────────────────────────────────────────────┘
             X=0mm                     X=51mm                                  X=130mm
```

---

## 4. Recommended 3D Print Settings

| Setting | FDM Recommendation | SLA / Resin Recommendation |
| :--- | :--- | :--- |
| **Material** | PETG or PLA+ (High stiffness) | Tough / ABS-Like Resin |
| **Nozzle Diameter** | $0.4\text{ mm}$ | N/A |
| **Layer Height** | $0.20\text{ mm}$ (Quality) | $0.05\text{ mm}$ |
| **Wall Perimeters** | $4 - 6$ perimeters ($1.6 - 2.4\text{ mm}$) | Solid walls |
| **Top / Bottom Layers**| $5$ top / $5$ bottom layers | Solid shells |
| **Infill Density** | $25\% - 40\%$ Gyroid or Grid | Solid or hollowed with drain holes |
| **Supports** | None required (All overhangs $\le 45^\circ$, bridges $\le 14\text{ mm}$) | Standard touchpoint supports |
| **Print Orientation** | **Bottom Case**: Flat on base<br>**Top Case**: Flat on faceplate | Flat on buildplate |

---

## 5. Bill of Materials (BOM) & Fasteners

* **1x** 3D-Printed Bottom Case Half (`output/base.stl`)
* **1x** 3D-Printed Top Case Half (`output/lid.stl`)
* **5x** Standard $6\times 6\times 5\text{ mm}$ Tactile Pushbuttons (Through-hole with $3.5\text{ mm}$ round button stem)
* **4x** M3 $\times 20\text{ mm}$ Socket Head Cap Screws (Enclosure corner assembly)
* **4x** M3 Brass Heat-Set Threaded Inserts (OD $4.6\text{ mm}$, Length $4\dots 5\text{ mm}$) — *or M3 self-tapping screws directly into plastic*
* **6x** M2 $\times 4\text{ mm}$ or M2 $\times 6\text{ mm}$ Screws (Fastening UDOO KEY to bottom standoffs)
* **4x** M2 $\times 4\text{ mm}$ Screws (Securing ST7735 display to top case frame)
* **1x** UDOO KEY Development Board
* **1x** 1.44" ST7735 128x128 SPI TFT Display Module

---

## 6. Assembly Instructions

1. **Test Fit Calibration**:
   * Print `output/test_coupon.stl` first (~10 minutes).
   * Verify M2 standoff screws, M3 corner screws, USB-C cable clearance, and press-fit one $6\times 6\text{ mm}$ tactile switch into the underside test pocket to confirm thumb feel in the concave dish.
2. **Bottom Case Prep**:
   * Inspect the 6 raised standoffs. Pre-thread with an M2 screw or tap lightly.
   * Lower the UDOO KEY PCB into the bottom case, resting on the 6 standoffs.
   * Secure the PCB with 6x M2 screws.
3. **Top Case & Faceplate Prep**:
   * Install 4x M3 brass heat-set inserts into the corner bosses using a soldering iron at ~220°C.
   * Press-fit the 5 tactile switches into their retention cradles on the underside of the lid until their metal plates sit flush against the ceiling.
   * Route the switch wire leads through the side relief channels and connect to the RP2040 GPIO headers (UP, DOWN, LEFT, RIGHT, SELECT + GND).
   * Place the ST7735 display module into the internal bezel pocket. Secure with M2 screws.
   * Connect the wiring harness from the display to the UDOO KEY headers.
4. **Final Closure**:
   * Mate the top case onto the bottom case, ensuring the perimeter interlocking lip engages smoothly.
   * Insert 4x M3 $\times 20\text{ mm}$ screws through the bottom counterbores and tighten into the top case brass inserts.
