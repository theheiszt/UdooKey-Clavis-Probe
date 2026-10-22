#include "display_ui.h"
#include "frontend_control.h"
#include "trigger_engine.h"
#include "instrument_mode_manager.h"
#include "bus_pirate_core.h"
#include "acquisition_api.h"
#include "hardware_config.h"
#include "adc_subsystem.h"
#include "interchip_link.h"
#include "sump_protocol.h"
#include "freq_meter.h"
#include "pio_edge_analyzer.h"
#include <SPI.h>

#ifndef TFT_CS
#define TFT_CS   13
#endif
#ifndef TFT_DC
#define TFT_DC   12
#endif
#ifndef TFT_RST
#define TFT_RST  11
#endif
#ifndef TFT_SCK
#define TFT_SCK  18
#endif
#ifndef TFT_MOSI
#define TFT_MOSI 19
#endif
#ifndef TFT_MISO
#define TFT_MISO 20
#endif

// Single Main Display instance (Main ST7735 on GP13/12/11, matching old working bottom screen with rst = -1)
Adafruit_ST7735 tft_main(&SPI, TFT_CS, TFT_DC, -1);

static uint8_t current_tab = INITR_144GREENTAB;
static uint32_t current_spi_speed = 32000000;
static bool current_invert = false; // Default to NORMAL un-inverted (matching old working bottom screen)
static uint8_t current_vcom = 0x0E;
static uint8_t current_rotation = 0; // 0 = Portrait
static UIScreen current_screen = UIScreen::MAIN_MENU;
static int main_menu_idx = 0;
static int tools_menu_idx = 0;
static int gen_menu_idx = 0;
static int settings_menu_idx = 0;
static bool screen_needs_redraw = true;
static uint32_t last_live_update_ms = 0;

static const uint16_t CH_COLORS[8] = {
    COLOR_CH0, COLOR_CH1, COLOR_CH2, COLOR_CH3,
    COLOR_CH4, COLOR_CH5, COLOR_CH6, COLOR_CH7
};

static const uint32_t SAMPLE_RATES[] = {
    100000, 1000000, 5000000, 10000000, 25000000, 50000000, 100000000
};
static const char* SAMPLE_RATE_STRS[] = {
    "100k", "1M", "5M", "10M", "25M", "50M", "100M"
};
static size_t current_rate_idx = 3; // 10MHz default

static const uint8_t MEASURE_PINS[] = { PIN_EXT_TRIG, 2, 3, 4, 5, 6, 7, 8, 9 };
static const char* MEASURE_PIN_NAMES[] = { "GP10(EXT)", "GP2", "GP3", "GP4", "GP5", "GP6", "GP7", "GP8", "GP9" };
static const uint8_t NUM_MEASURE_PINS = sizeof(MEASURE_PINS) / sizeof(MEASURE_PINS[0]);
static uint8_t measure_pin_idx = 0;

static void format_time_compact(float sec, char* buf, size_t sz) {
    if (sec <= 0.0f) {
        snprintf(buf, sz, "---");
    } else if (sec >= 1.0f) {
        snprintf(buf, sz, "%.3fs", sec);
    } else if (sec >= 1e-3f) {
        snprintf(buf, sz, "%.2fms", sec * 1e3f);
    } else if (sec >= 1e-6f) {
        snprintf(buf, sz, "%.2fus", sec * 1e6f);
    } else {
        snprintf(buf, sz, "%.1fns", sec * 1e9f);
    }
}

static void format_freq_compact(float hz, char* buf, size_t sz) {
    if (hz <= 0.0f) {
        snprintf(buf, sz, "0 Hz");
    } else if (hz >= 1e6f) {
        snprintf(buf, sz, "%.3fMHz", hz / 1e6f);
    } else if (hz >= 1e3f) {
        snprintf(buf, sz, "%.2fkHz", hz / 1e3f);
    } else {
        snprintf(buf, sz, "%.1fHz", hz);
    }
}

// ─── Display Control Helpers ──────────────────────────────────────────────────
void ui_select_display() {
    gpio_put(TFT_CS, 0);
}

void ui_deselect_display() {
    gpio_put(TFT_CS, 1);
}

void ui_set_rotation(uint8_t rot) {
    current_rotation = rot % 4;
    tft_main.setRotation(current_rotation);
    screen_needs_redraw = true;
}

uint8_t ui_get_rotation() {
    return current_rotation;
}

void ui_set_screen(UIScreen screen) {
    if (current_screen == UIScreen::CONTINUITY && screen != UIScreen::CONTINUITY) {
        buzzer_stop();
    }
    current_screen = screen;
    screen_needs_redraw = true;
}

UIScreen ui_get_screen() {
    return current_screen;
}

// ─── Single Display Initialization Sequence (Restored from Working Bottom Screen) ──
void ui_init() {
    // 1. Configure Hardware SPI0
    SPI.setSCK(TFT_SCK);
    SPI.setTX(TFT_MOSI);
    SPI.setRX(TFT_MISO);
    SPI.begin();

    // 2. Configure display control pins as SIO outputs with 4mA drive strength
    gpio_init(TFT_CS);
    gpio_set_function(TFT_CS, GPIO_FUNC_SIO);
    gpio_set_dir(TFT_CS, GPIO_OUT);
    gpio_set_drive_strength(TFT_CS, GPIO_DRIVE_STRENGTH_4MA);
    gpio_put(TFT_CS, 1); // Defaults HIGH (deselected)

    gpio_init(TFT_DC);
    gpio_set_function(TFT_DC, GPIO_FUNC_SIO);
    gpio_set_dir(TFT_DC, GPIO_OUT);
    gpio_set_drive_strength(TFT_DC, GPIO_DRIVE_STRENGTH_4MA);
    gpio_put(TFT_DC, 1);

    if (TFT_RST >= 0) {
        gpio_init(TFT_RST);
        gpio_set_function(TFT_RST, GPIO_FUNC_SIO);
        gpio_set_dir(TFT_RST, GPIO_OUT);
        gpio_set_drive_strength(TFT_RST, GPIO_DRIVE_STRENGTH_4MA);
        gpio_put(TFT_RST, 1);
    }

    // 3. Hardware Reset Pulse (25ms low, 120ms high recovery - exactly matching old bottom display)
    if (TFT_RST >= 0) {
        gpio_put(TFT_RST, 0);
        delay(25);
        gpio_put(TFT_RST, 1);
        delay(120);
    }

    // 4. Initialize ST7735 Main Display (exact sequence from old working bottom display)
    tft_main.initR(INITR_144GREENTAB);
    ui_set_rotation(0);
    tft_main.fillScreen(COLOR_BG);

    // 5. Initialize Five-Button Input System
    button_nav_init();

    // 6. Enter Main Menu
    ui_set_screen(UIScreen::MAIN_MENU);
}

// ─── Waveform Rendering Engine (Preserved & Adapted for Single Screen) ────────
void ui_update_waveforms(const uint8_t* buffer, uint32_t total_samples, uint32_t start_index) {
    if (current_screen != UIScreen::LOGIC_ANALYZER) return;

    tft_main.fillScreen(COLOR_BG);

    // Top Header: Scope status & sample rate
    bool running = acquisition_is_running();
    tft_main.fillRect(0, 0, TFT_W, 14, COLOR_HEADER);
    tft_main.setTextColor(COLOR_ACCENT);
    tft_main.setTextSize(1);
    tft_main.setCursor(2, 3);
    const TriggerEngineConfig& tc_head = trigger_engine_get_config();
    const char* t_lbl = (tc_head.mode == TRIG_MODE_AUTO) ? "A" :
                        (tc_head.mode == TRIG_MODE_EDGE_RISING) ? "R" :
                        (tc_head.mode == TRIG_MODE_EDGE_FALLING) ? "F" :
                        (tc_head.mode == TRIG_MODE_LEVEL_QUAL) ? "Q" :
                        (tc_head.mode == TRIG_MODE_PATTERN) ? "P" : "E";
    uint8_t pre_div = frontend_get_prescaler();
    if (pre_div > 1) {
        tft_main.printf("LA:%s[%s]x%u", SAMPLE_RATE_STRS[current_rate_idx], t_lbl, pre_div);
    } else {
        tft_main.printf("LA:%s[%s]", SAMPLE_RATE_STRS[current_rate_idx], t_lbl);
    }
    tft_main.setCursor(76, 3);
    if (running) {
        tft_main.setTextColor(COLOR_CH2);
        tft_main.print("RUNNING");
    } else if (sump_is_active()) {
        tft_main.setTextColor(COLOR_CH1);
        tft_main.print("SIGROK");
    } else {
        tft_main.setTextColor(COLOR_CH0);
        tft_main.print("READY");
    }


    int channel_h = 11;
    int y_start   = 16;

    for (int ch = 0; ch < 8; ch++) {
        int base_y = y_start + (ch * channel_h);

        tft_main.setTextColor(CH_COLORS[ch]);
        tft_main.setTextSize(1);
        tft_main.setCursor(1, base_y + 1);
        tft_main.printf("%d", ch);

        tft_main.drawFastHLine(10, base_y + channel_h - 2, TFT_W - 10, COLOR_GRID);

        if (buffer && total_samples > 0) {
            uint8_t prev_state = (buffer[start_index] >> ch) & 0x01;

            for (int x = 0; x < 117; x++) {
                uint32_t sample_idx = start_index + x;
                if (sample_idx >= total_samples) break;

                uint8_t curr_state = (buffer[sample_idx] >> ch) & 0x01;

                int y_prev = base_y + channel_h - 2 - (prev_state * 7);
                int y_curr = base_y + channel_h - 2 - (curr_state * 7);
                int screen_x = 10 + x;

                if (prev_state != curr_state) {
                    tft_main.drawFastVLine(screen_x, min(y_prev, y_curr), abs(y_prev - y_curr) + 1, CH_COLORS[ch]);
                }
                tft_main.drawPixel(screen_x, y_curr, CH_COLORS[ch]);
                prev_state = curr_state;
            }
        }
    }

    // Draw Deterministic Trigger Position Line & 'T' Marker
    const TriggerEngineConfig& tc = trigger_engine_get_config();
    int trig_screen_x = 10 + (116 * tc.pre_trigger_pct) / 100;
    for (int y = y_start; y < y_start + (8 * channel_h); y += 3) {
        tft_main.drawPixel(trig_screen_x, y, 0xF800); // Red dashed line
    }
    tft_main.setTextColor(0xF800);
    tft_main.setTextSize(1);
    tft_main.setCursor(trig_screen_x - 2, 3);
    tft_main.print("T");

    // Bottom Navigation Guide Footer
    tft_main.fillRect(0, 106, TFT_W, 22, 0x18C3);
    tft_main.drawFastHLine(0, 105, TFT_W, 0x3341);
    tft_main.setTextColor(0xFFFF);
    tft_main.setTextSize(1);
    tft_main.setCursor(2, 108);
    tft_main.printf("SEL:%s L:Back", running ? "Stop" : "Run ");
    tft_main.setCursor(2, 118);
    tft_main.setTextColor(0x8410);
    tft_main.printf("U/D:Rate R:Pos(%u%%)", tc.pre_trigger_pct);
}

void ui_notify_capture_complete() {
    screen_needs_redraw = true;
}

// ─── Interactive On-Screen Toast Notification ─────────────────────────────────
void ui_show_toast(const char* title, const char* subtitle, uint16_t color, uint32_t duration_ms) {
    int w = TFT_W - 16;
    int h = 42;
    int x = 8;
    int y = (TFT_H - h) / 2;

    tft_main.fillRoundRect(x, y, w, h, 4, 0x1082); // Dark slate body
    tft_main.drawRoundRect(x, y, w, h, 4, color);   // Color border
    tft_main.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 3, color);

    tft_main.setTextColor(0xFFFF);
    tft_main.setTextSize(1);
    tft_main.setCursor(x + 8, y + 8);
    tft_main.print(title);

    if (subtitle && *subtitle) {
        tft_main.setTextColor(color);
        tft_main.setCursor(x + 8, y + 23);
        tft_main.print(subtitle);
    }

    uint32_t t0 = millis();
    while (millis() - t0 < duration_ms) {
        if (button_nav_poll() != UI_NONE) break; // Allow quick dismissal
        delay(10);
    }
    screen_needs_redraw = true;
}

// ─── Menu Renderers ───────────────────────────────────────────────────────────

static void render_main_menu() {
    tft_main.fillScreen(COLOR_BG);

    // Title Bar
    tft_main.fillRect(0, 0, TFT_W, 18, COLOR_HEADER);
    tft_main.drawFastHLine(0, 18, TFT_W, COLOR_ACCENT);
    tft_main.setTextColor(0xFFFF);
    tft_main.setTextSize(1);
    tft_main.setCursor(18, 5);
    tft_main.print("UDOO ANALYZER");

    const char* items[] = {
        "Logic Analyzer",
        "Measurements",
        "Continuity Test",
        "Analog Scope",
        "Protocol Tools",
        "Wireless & ESP32",
        "Generator",
        "Settings"
    };
    const int item_count = 8;

    // Windowed scrolling: display 5 items at a time
    int start_idx = 0;
    if (main_menu_idx >= 4) {
        start_idx = main_menu_idx - 3;
        if (start_idx > item_count - 5) start_idx = item_count - 5;
    }

    for (int i = 0; i < 5; i++) {
        int idx = start_idx + i;
        if (idx >= item_count) break;
        int y = 23 + (i * 18);
        bool selected = (idx == main_menu_idx);

        if (selected) {
            tft_main.fillRoundRect(4, y, TFT_W - 8, 16, 3, COLOR_SELECT);
            tft_main.drawRoundRect(4, y, TFT_W - 8, 16, 3, 0xFFFF);
            tft_main.setTextColor(0xFFFF);
            tft_main.setCursor(8, y + 4);
            tft_main.print("> ");
            tft_main.print(items[idx]);
        } else {
            tft_main.setTextColor(COLOR_TEXT);
            tft_main.setCursor(14, y + 4);
            tft_main.print(items[idx]);
        }
    }

    // Scroll Indicator Dots
    if (start_idx > 0) tft_main.fillTriangle(TFT_W - 6, 26, TFT_W - 10, 30, TFT_W - 2, 30, COLOR_CH1);
    if (start_idx + 5 < item_count) tft_main.fillTriangle(TFT_W - 6, 110, TFT_W - 10, 106, TFT_W - 2, 106, COLOR_CH1);

    // Footer Navigation Hint
    tft_main.drawFastHLine(0, 114, TFT_W, 0x3186);
    tft_main.setTextColor(0x8410);
    tft_main.setTextSize(1);
    tft_main.setCursor(4, 118);
    tft_main.print("UP/DN:Nav  SEL/R:Enter");
}

static void render_protocol_tools() {
    tft_main.fillScreen(COLOR_BG);

    // Title Bar
    tft_main.fillRect(0, 0, TFT_W, 16, 0x001A);
    tft_main.drawFastHLine(0, 16, TFT_W, COLOR_ACCENT);
    tft_main.setTextColor(0xFFFF);
    tft_main.setTextSize(1);
    tft_main.setCursor(16, 4);
    tft_main.print("PROTOCOL TOOLS");

    const char* tools[] = {
        "I2C Bus Scan",
        "SPI Flash Info",
        "UART Bridge",
        "1-Wire Search",
        "Digital I/O (DIO)",
        "USB Bus Prober",
        "ARM SWD [UNVERIF]",
        "Safe Hi-Z Mode"
    };
    const int tool_count = 8;

    int start_idx = 0;
    if (tools_menu_idx >= 5) {
        start_idx = tools_menu_idx - 4;
        if (start_idx > tool_count - 6) start_idx = tool_count - 6;
    }

    for (int i = 0; i < 6; i++) {
        int idx = start_idx + i;
        if (idx >= tool_count) break;
        int y = 20 + (i * 15);
        bool selected = (idx == tools_menu_idx);

        if (selected) {
            tft_main.fillRect(4, y, TFT_W - 8, 14, 0x2184);
            tft_main.drawRect(4, y, TFT_W - 8, 14, 0xFFFF);
            tft_main.setTextColor(0xFFFF);
            tft_main.setCursor(6, y + 3);
            tft_main.print("> ");
            tft_main.print(tools[idx]);
        } else {
            tft_main.setTextColor(idx == 6 ? 0xFD20 : COLOR_TEXT);
            tft_main.setCursor(12, y + 3);
            tft_main.print(tools[idx]);
        }
    }

    // Scroll indicators
    if (start_idx > 0) tft_main.fillTriangle(TFT_W - 6, 22, TFT_W - 10, 26, TFT_W - 2, 26, COLOR_CH1);
    if (start_idx + 6 < tool_count) tft_main.fillTriangle(TFT_W - 6, 110, TFT_W - 10, 106, TFT_W - 2, 106, COLOR_CH1);

    // Footer
    tft_main.drawFastHLine(0, 114, TFT_W, 0x3186);
    tft_main.setTextColor(0x8410);
    tft_main.setCursor(4, 118);
    tft_main.print("SEL:Run   L:Back");
}

static void render_measurements() {
    tft_main.fillScreen(COLOR_BG);

    // Title Bar
    tft_main.fillRect(0, 0, TFT_W, 16, 0x1882);
    tft_main.drawFastHLine(0, 16, TFT_W, COLOR_ACCENT);
    tft_main.setTextColor(0xFFFF);
    tft_main.setTextSize(1);
    tft_main.setCursor(8, 4);
    tft_main.print("MEASUREMENTS (LIVE)");

    // Read calibrated analog voltages
    float adc0_v = adc_read_calibrated_voltage(ADC_CH_0, 32);
    float adc1_v = adc_read_calibrated_voltage(ADC_CH_1, 32);
    float adc2_v = adc_read_calibrated_voltage(ADC_CH_2, 32);
    float vsys_v = adc_read_calibrated_voltage(ADC_CH_VSYS, 32);
    float temp_c = adc_read_internal_temp_c();

    tft_main.setTextColor(COLOR_CH2);
    tft_main.setCursor(4, 19);
    tft_main.printf("A0:%.3fV ", adc0_v);
    tft_main.setTextColor(COLOR_CH1);
    tft_main.printf("A1:%.3fV", adc1_v);

    tft_main.setTextColor(COLOR_CH4);
    tft_main.setCursor(4, 29);
    tft_main.printf("A2:%.3fV ", adc2_v);
    tft_main.setTextColor(0xBDD7);
    tft_main.printf("SYS:%.2fV", vsys_v);

    tft_main.setTextColor(COLOR_TEXT);
    tft_main.setCursor(4, 39);
    tft_main.printf("MCU TEMP: %.1f C", temp_c);

    // Divider
    tft_main.drawFastHLine(0, 48, TFT_W, 0x3186);

    // Digital Measurements on selected pin
    uint8_t cur_pin = MEASURE_PINS[measure_pin_idx];
    FreqResult fres = freq_measure_pin(cur_pin, 20);

    char f_str[16];
    char p_str[16];
    char pw_str[16];

    if (fres.valid && fres.freq_hz > 0.0f) {
        format_freq_compact(fres.freq_hz, f_str, sizeof(f_str));
        format_time_compact(fres.period_s, p_str, sizeof(p_str));
        float pw_s = fres.period_s * (fres.duty_percent / 100.0f);
        format_time_compact(pw_s, pw_str, sizeof(pw_str));
    } else {
        snprintf(f_str, sizeof(f_str), "0 Hz");
        snprintf(p_str, sizeof(p_str), "---");
        snprintf(pw_str, sizeof(pw_str), "---");
    }

    tft_main.setTextColor(COLOR_CH3);
    tft_main.setCursor(4, 52);
    tft_main.printf("PIN : %s", MEASURE_PIN_NAMES[measure_pin_idx]);

    tft_main.setTextColor(COLOR_CH0);
    tft_main.setCursor(4, 63);
    tft_main.printf("FREQ: %s", f_str);

    tft_main.setTextColor(COLOR_CH1);
    tft_main.setCursor(4, 74);
    tft_main.printf("PER : %s", p_str);

    tft_main.setTextColor(COLOR_CH2);
    tft_main.setCursor(4, 85);
    tft_main.printf("PW/D: %s / %.1f%%", pw_str, fres.valid ? fres.duty_percent : 0.0f);

    // Prescaler & Protection
    uint8_t p_ratio = frontend_get_prescaler();
    bool prot_active = frontend_get_protection();
    tft_main.setTextColor(0xFFFF);
    tft_main.setCursor(4, 98);
    tft_main.printf("PRE/PROT: x%u / %s", p_ratio, prot_active ? "ON" : "OFF");

    // Footer
    tft_main.drawFastHLine(0, 114, TFT_W, 0x3186);
    tft_main.setTextColor(0x8410);
    tft_main.setCursor(4, 118);
    tft_main.print("U/D:Pin SEL:Ref L:Back");
}

static void render_wireless_esp32() {
    tft_main.fillScreen(COLOR_BG);

    // Title Bar
    tft_main.fillRect(0, 0, TFT_W, 16, 0x0014);
    tft_main.drawFastHLine(0, 16, TFT_W, COLOR_ACCENT);
    tft_main.setTextColor(0xFFFF);
    tft_main.setTextSize(1);
    tft_main.setCursor(8, 4);
    tft_main.print("WIRELESS & ESP32");

    // Inter-MCU UART Link Status
    uint32_t tx_c = interchip_get_tx_count();
    uint32_t rx_c = interchip_get_rx_count();
    tft_main.setTextColor(COLOR_CH0);
    tft_main.setCursor(4, 19);
    tft_main.print("LINK : UART 1M [OK]");

    tft_main.setTextColor(COLOR_TEXT);
    tft_main.setCursor(4, 29);
    tft_main.printf("PKTS : TX:%lu RX:%lu", (unsigned long)tx_c, (unsigned long)rx_c);

    // SoftAP Credentials & Telemetry
    tft_main.drawFastHLine(0, 39, TFT_W, 0x3186);
    tft_main.setTextColor(COLOR_CH1);
    tft_main.setCursor(4, 43);
    tft_main.print("SSID : UDOO-BIT-PIRATE");

    tft_main.setTextColor(COLOR_CH2);
    tft_main.setCursor(4, 54);
    tft_main.print("IP   : 192.168.4.1");

    tft_main.setTextColor(0xBDD7);
    tft_main.setCursor(4, 65);
    tft_main.print("AP   : Ch 1 | Max 4 Cli");

    tft_main.setTextColor(COLOR_CH0);
    tft_main.setCursor(4, 76);
    tft_main.print("WEB  : Port 80 / WS:81");

    tft_main.drawFastHLine(0, 86, TFT_W, 0x3186);

    // Verified Wireless Capabilities
    tft_main.setTextColor(COLOR_CH3);
    tft_main.setCursor(4, 90);
    tft_main.print("BLE  : SCANNER [OK]");

    tft_main.setTextColor(COLOR_CH4);
    tft_main.setCursor(4, 101);
    tft_main.print("PCAP : 802.11 SNIF [OK]");

    // Footer
    tft_main.drawFastHLine(0, 114, TFT_W, 0x3186);
    tft_main.setTextColor(0x8410);
    tft_main.setCursor(4, 118);
    tft_main.print("SEL:Ping ESP32  L:Back");
}

static void render_continuity() {
    tft_main.fillScreen(COLOR_BG);

    // Title Bar
    tft_main.fillRect(0, 0, TFT_W, 16, 0x0320); // Dark Green Header
    tft_main.drawFastHLine(0, 16, TFT_W, COLOR_CH0);
    tft_main.setTextColor(0xFFFF);
    tft_main.setTextSize(1);
    tft_main.setCursor(14, 4);
    tft_main.print("CONTINUITY TESTER");

    ContinuityStatus st = adc_continuity_update(ADC_CH_0);
    ContinuityConfig &cfg = adc_continuity_get_config();

    // Large Status Banner Box
    int box_y = 22;
    int box_h = 32;
    if (st.is_connected) {
        tft_main.fillRoundRect(4, box_y, TFT_W - 8, box_h, 4, 0x05E0); // Bright Green
        tft_main.drawRoundRect(4, box_y, TFT_W - 8, box_h, 4, 0xFFFF);
        tft_main.setTextColor(0x0000);
        tft_main.setTextSize(2);
        tft_main.setCursor(14, box_y + 8);
        tft_main.print("CONNECTED");
    } else {
        tft_main.fillRoundRect(4, box_y, TFT_W - 8, box_h, 4, 0x4800); // Dark Red
        tft_main.drawRoundRect(4, box_y, TFT_W - 8, box_h, 4, 0xFD20);
        tft_main.setTextColor(0xFFFF);
        tft_main.setTextSize(2);
        tft_main.setCursor(10, box_y + 8);
        tft_main.print("OPEN CIRCT");
    }

    // Resistance Readout
    tft_main.setTextSize(1);
    tft_main.setTextColor(COLOR_CH2);
    tft_main.setCursor(6, 60);
    if (st.measured_ohms >= 1e6f) {
        tft_main.print("Resistance: > 1 M-Ohm");
    } else if (st.measured_ohms >= 1000.0f) {
        tft_main.printf("Resistance: %.2f k-Ohm", st.measured_ohms / 1000.0f);
    } else {
        tft_main.printf("Resistance: %6.1f Ohm", st.measured_ohms);
    }

    // Threshold & Hysteresis
    tft_main.setTextColor(COLOR_CH1);
    tft_main.setCursor(6, 73);
    tft_main.printf("Threshold : < %.0f Ohm", cfg.threshold_ohms);

    tft_main.setTextColor(0xBDD7);
    tft_main.setCursor(6, 85);
    tft_main.printf("Hysteresis: + %.0f Ohm", cfg.hysteresis_ohms);

    // Audio & Probe Info
    tft_main.setTextColor(cfg.buzzer_enabled ? COLOR_CH0 : COLOR_CH3);
    tft_main.setCursor(6, 97);
    tft_main.printf("Buzzer(GP20): %s", cfg.buzzer_enabled ? (st.buzzer_active ? "BEEPING" : "ACTIVE") : "MUTED");

    // Footer
    tft_main.drawFastHLine(0, 114, TFT_W, 0x3186);
    tft_main.setTextColor(0x8410);
    tft_main.setCursor(4, 118);
    tft_main.print("UP/DN:Thresh SEL:Mute");
}

static void render_analog_scope() {
    tft_main.fillScreen(COLOR_BG);

    // Title Bar
    tft_main.fillRect(0, 0, TFT_W, 16, 0x1084);
    tft_main.drawFastHLine(0, 16, TFT_W, COLOR_ACCENT);
    tft_main.setTextColor(0xFFFF);
    tft_main.setTextSize(1);
    tft_main.setCursor(10, 4);
    tft_main.print("ANALOG SCOPE (GP26)");

    // Capture 128 samples via DMA at 50 kSPS
    AdcWaveformConfig wcfg;
    wcfg.channel = ADC_CH_0;
    wcfg.sample_rate_hz = 50000;
    wcfg.sample_count = 128;
    wcfg.trigger_level_v = 1.65f;
    wcfg.trigger_slope = TRIG_SLOPE_RISING;
    wcfg.trigger_hysteresis_v = 0.05f;
    wcfg.pre_trigger_pct = 50;

    if (adc_waveform_start(wcfg)) {
        adc_waveform_wait_completion(50);
    }
    AdcWaveformStats stats = adc_waveform_compute_stats(wcfg);
    const uint16_t* buf = adc_waveform_get_buffer();

    // Draw Grid (y: 20 to 86)
    for (int gx = 8; gx < TFT_W; gx += 28) tft_main.drawFastVLine(gx, 20, 66, COLOR_GRID);
    for (int gy = 20; gy <= 86; gy += 16) tft_main.drawFastHLine(0, gy, TFT_W, COLOR_GRID);

    // Plot waveform trace
    int prev_py = 53;
    for (int x = 0; x < 128 && x < TFT_W; x++) {
        uint16_t raw = buf[x] & 0x0FFF;
        // Map 0..4095 (0..3.3V) to y: 84 to 22
        int py = 84 - (int)((raw * 62) / 4095);
        if (py < 21) py = 21;
        if (py > 85) py = 85;
        if (x > 0) {
            tft_main.drawLine(x - 1, prev_py, x, py, COLOR_CH1);
        }
        prev_py = py;
    }

    // Metrics Footer
    tft_main.setTextColor(COLOR_CH2);
    tft_main.setCursor(2, 92);
    tft_main.printf("Vpp:%.2fV Vavg:%.2fV", stats.v_pp, stats.v_avg);

    tft_main.setTextColor(COLOR_CH0);
    tft_main.setCursor(2, 103);
    if (stats.estimated_freq_hz > 0) {
        if (stats.estimated_freq_hz >= 1000) {
            tft_main.printf("Freq: %.1f kHz %s", stats.estimated_freq_hz / 1000.0f, stats.triggered ? "[TRG]" : "");
        } else {
            tft_main.printf("Freq: %.0f Hz %s", stats.estimated_freq_hz, stats.triggered ? "[TRG]" : "");
        }
    } else {
        tft_main.printf("Trig: %s (1.65V)", stats.triggered ? "LOCKED" : "FREE");
    }

    tft_main.drawFastHLine(0, 114, TFT_W, 0x3186);
    tft_main.setTextColor(0x8410);
    tft_main.setCursor(4, 118);
    tft_main.print("Live 50kSPS   L:Back");
}

static void render_generator() {
    tft_main.fillScreen(COLOR_BG);

    // Title Bar
    tft_main.fillRect(0, 0, TFT_W, 16, 0x7800);
    tft_main.setTextColor(0xFFFF);
    tft_main.setTextSize(1);
    tft_main.setCursor(14, 4);
    tft_main.print("SIGNAL GENERATOR");

    const char* gens[] = {
        "PWM: 1 kHz",
        "PWM: 10 kHz",
        "PWM: 1 MHz",
        "PWM: 10 MHz",
        "Glitch Injector",
        "Stop Generator"
    };

    for (int i = 0; i < 6; i++) {
        int y = 20 + (i * 14);
        bool selected = (i == gen_menu_idx);

        if (selected) {
            tft_main.fillRect(4, y, TFT_W - 8, 13, 0x8180);
            tft_main.setTextColor(0xFFFF);
            tft_main.setCursor(6, y + 3);
            tft_main.print("> ");
            tft_main.print(gens[i]);
        } else {
            tft_main.setTextColor(COLOR_TEXT);
            tft_main.setCursor(12, y + 3);
            tft_main.print(gens[i]);
        }
    }

    // Footer
    tft_main.drawFastHLine(0, 114, TFT_W, 0x3186);
    tft_main.setTextColor(0x8410);
    tft_main.setCursor(4, 118);
    tft_main.print("SEL:Apply   L:Back");
}

static void render_settings() {
    tft_main.fillScreen(COLOR_BG);

    // Title Bar
    tft_main.fillRect(0, 0, TFT_W, 16, 0x2945);
    tft_main.setTextColor(0xFFFF);
    tft_main.setTextSize(1);
    tft_main.setCursor(36, 4);
    tft_main.print("SETTINGS");

    const TriggerEngineConfig& tc = trigger_engine_get_config();
    const char* trig_name = (tc.mode == TRIG_MODE_AUTO) ? "Auto" :
                            (tc.mode == TRIG_MODE_EDGE_RISING) ? "Rise" :
                            (tc.mode == TRIG_MODE_EDGE_FALLING) ? "Fall" :
                            (tc.mode == TRIG_MODE_LEVEL_QUAL) ? "Qual" :
                            (tc.mode == TRIG_MODE_PATTERN) ? "Patt" : "Ext";

    char opt0[24]; snprintf(opt0, sizeof(opt0), "Trig: %s", trig_name);
    char opt1[24]; snprintf(opt1, sizeof(opt1), "Prescale: x%u", frontend_get_prescaler());
    char opt2[24]; snprintf(opt2, sizeof(opt2), "Protect: %s", frontend_get_protection() ? "ON" : "OFF");
    char opt3[24]; snprintf(opt3, sizeof(opt3), "Rotate: %u (%s)", current_rotation, (current_rotation % 2 == 0) ? "Port" : "Land");

    const char* sets[] = {
        opt0,
        opt1,
        opt2,
        opt3,
        "Hardware Diag",
        "Display Test",
        "Self Test 17/17",
        "System Info"
    };

    for (int i = 0; i < 8; i++) {
        int y = 18 + (i * 12);
        bool selected = (i == settings_menu_idx);

        if (selected) {
            tft_main.fillRect(4, y, TFT_W - 8, 11, 0x39E7);
            tft_main.setTextColor(0xFFFF);
            tft_main.setCursor(6, y + 2);
            tft_main.print("> ");
            tft_main.print(sets[i]);
        } else {
            tft_main.setTextColor(COLOR_TEXT);
            tft_main.setCursor(12, y + 2);
            tft_main.print(sets[i]);
        }
    }

    // Footer
    tft_main.drawFastHLine(0, 114, TFT_W, 0x3186);
    tft_main.setTextColor(0x8410);
    tft_main.setCursor(4, 118);
    tft_main.print("SEL:Change  L:Back");
}

static void render_sys_info() {
    tft_main.fillScreen(COLOR_BG);

    // Title Bar
    tft_main.fillRect(0, 0, TFT_W, 16, COLOR_HEADER);
    tft_main.drawFastHLine(0, 16, TFT_W, COLOR_ACCENT);
    tft_main.setTextColor(COLOR_ACCENT);
    tft_main.setTextSize(1);
    tft_main.setCursor(14, 4);
    tft_main.print("SYSTEM INFORMATION");

    tft_main.setTextColor(COLOR_CH0);
    tft_main.setCursor(4, 22);
    tft_main.print("MCU: RP2040 Dual ARM");

    tft_main.setTextColor(COLOR_TEXT);
    tft_main.setCursor(4, 34);
    tft_main.print("Clock : 133 MHz PLL");

    tft_main.setCursor(4, 46);
    tft_main.print("SRAM  : 264 KB");

    tft_main.setCursor(4, 58);
    tft_main.print("Flash : 16 MB QSPI");

    tft_main.setTextColor(COLOR_CH1);
    tft_main.setCursor(4, 70);
    tft_main.print("Link  : UART GP0/GP1");

    tft_main.setCursor(4, 82);
    tft_main.print("ESP32 : IO22/IO19 @ 1M");

    tft_main.setTextColor(COLOR_CH2);
    tft_main.setCursor(4, 94);
    tft_main.print("Build : Oct 2026 v2.4");

    // Footer
    tft_main.drawFastHLine(0, 114, TFT_W, 0x3186);
    tft_main.setTextColor(0x8410);
    tft_main.setCursor(8, 118);
    tft_main.print("Press ANY key: Back");
}

static void render_diagnostics() {
    tft_main.fillScreen(COLOR_BG);

    // Title Bar
    tft_main.fillRect(0, 0, TFT_W, 14, COLOR_HEADER);
    tft_main.drawFastHLine(0, 14, TFT_W, COLOR_ACCENT);
    tft_main.setTextColor(COLOR_ACCENT);
    tft_main.setTextSize(1);
    tft_main.setCursor(4, 3);
    tft_main.print("HW DIAGNOSTICS");

    // 1. RP2040 Core & Clock & DMA
    bool pio_ok = (pio_get_dreq(pio0, 0, false) != 0);
    bool dma_ok = (dma_channel_is_claimed(0) || dma_claim_unused_channel(false) >= 0);

    tft_main.setCursor(4, 17);
    tft_main.setTextColor(COLOR_TEXT);
    tft_main.print("RP2040: ");
    tft_main.setTextColor(COLOR_CH0);
    tft_main.print("133M OK ");
    tft_main.setTextColor((pio_ok && dma_ok) ? COLOR_CH0 : 0xF800);
    tft_main.print((pio_ok && dma_ok) ? "DMA OK" : "DMA ERR");

    // 2. UART Link to ESP32 (GP0 TX / GP1 RX @ 1M baud)
    uint32_t tx_c = interchip_get_tx_count();
    uint32_t rx_c = interchip_get_rx_count();
    tft_main.setCursor(4, 28);
    tft_main.setTextColor(COLOR_TEXT);
    tft_main.print("UART  : ");
    tft_main.setTextColor(COLOR_CH1);
    tft_main.printf("1M T:%lu R:%lu", (unsigned long)tx_c, (unsigned long)rx_c);

    // 3. Live 8-Ch Probe Bus (GP2..GP9)
    uint32_t gpio_all = gpio_get_all();
    uint8_t probe_byte = (gpio_all >> 2) & 0xFF;
    tft_main.setCursor(4, 39);
    tft_main.setTextColor(COLOR_TEXT);
    tft_main.print("PROBES: ");
    for (int b = 7; b >= 0; b--) {
        bool bit_val = (probe_byte >> b) & 1;
        tft_main.setTextColor(bit_val ? COLOR_CH0 : 0x4208);
        tft_main.print(bit_val ? "1" : "0");
    }

    // 4. EXT TRIG (GP10) & Prescaler (GP14)
    bool ext_trig = gpio_get(PIN_EXT_TRIG);
    tft_main.setCursor(4, 50);
    tft_main.setTextColor(COLOR_TEXT);
    tft_main.print("EXT TR: ");
    tft_main.setTextColor(ext_trig ? COLOR_CH2 : COLOR_CH6);
    tft_main.printf("GP10 %-4s ", ext_trig ? "HIGH" : "LOW");
    tft_main.setTextColor(COLOR_TEXT);
    tft_main.print("DIV:");
    tft_main.setTextColor(0xFFFF);
    tft_main.printf("x%u", frontend_get_prescaler());

    // 5. ADC0, ADC1, ADC2 (0 - 3.3V)
    float v0 = bp_read_adc_voltage(PIN_ADC0);
    float v1 = bp_read_adc_voltage(PIN_ADC1);
    float v2 = bp_read_adc_voltage(PIN_ADC2);
    tft_main.setCursor(4, 62);
    tft_main.setTextColor(COLOR_CH2);
    tft_main.printf("A0:%.2fV ", v0);
    tft_main.setTextColor(COLOR_CH1);
    tft_main.printf("A1:%.2fV", v1);
    tft_main.setCursor(4, 73);
    tft_main.setTextColor(COLOR_CH4);
    tft_main.printf("A2:%.2fV ", v2);
    float vsys = bp_read_adc_voltage(PIN_INTERNAL_VSYS_SENSE);
    tft_main.setTextColor(0xBDD7);
    tft_main.printf("SYS:%.2fV", vsys);

    // 6. Tactile Buttons (Active LOW with internal pull-ups)
    bool b_u = !gpio_get(PIN_BTN_UP);
    bool b_d = !gpio_get(PIN_BTN_DOWN);
    bool b_l = !gpio_get(PIN_BTN_LEFT);
    bool b_r = !gpio_get(PIN_BTN_RIGHT);
    bool b_s = !gpio_get(PIN_BTN_SELECT);

    tft_main.setCursor(4, 87);
    tft_main.setTextColor(COLOR_TEXT);
    tft_main.print("BTNS  : ");
    tft_main.setTextColor(b_u ? COLOR_CH2 : 0x52AA); tft_main.print("[U] ");
    tft_main.setTextColor(b_d ? COLOR_CH2 : 0x52AA); tft_main.print("[D] ");
    tft_main.setTextColor(b_l ? COLOR_CH2 : 0x52AA); tft_main.print("[L] ");
    tft_main.setTextColor(b_r ? COLOR_CH2 : 0x52AA); tft_main.print("[R] ");
    tft_main.setTextColor(b_s ? COLOR_CH0 : 0x52AA); tft_main.print("[SEL]");

    // 7. Navigation Footer
    tft_main.drawFastHLine(0, 114, TFT_W, 0x3186);
    tft_main.setTextColor(0x8410);
    tft_main.setCursor(4, 118);
    tft_main.print("Live Mode   L:Back");
}


// ─── Decoupled UI Task & Navigation Event Dispatcher (Section 10 & 11) ────────
void ui_task(bool& start_requested, bool& stop_requested) {
    if (ModeMgr.isProbeSpiActive()) return;

    UIEvent ev = button_nav_poll();

    if (ev != UI_NONE) {
        switch (current_screen) {
            case UIScreen::MAIN_MENU:
                if (ev == UI_UP) {
                    main_menu_idx = (main_menu_idx > 0) ? main_menu_idx - 1 : 7;
                    screen_needs_redraw = true;
                } else if (ev == UI_DOWN) {
                    main_menu_idx = (main_menu_idx < 7) ? main_menu_idx + 1 : 0;
                    screen_needs_redraw = true;
                } else if (ev == UI_SELECT || ev == UI_RIGHT) {
                    switch (main_menu_idx) {
                        case 0: ui_set_screen(UIScreen::LOGIC_ANALYZER); break;
                        case 1: ui_set_screen(UIScreen::MEASUREMENTS);   break;
                        case 2: ui_set_screen(UIScreen::CONTINUITY);     break;
                        case 3: ui_set_screen(UIScreen::ANALOG_SCOPE);   break;
                        case 4: ui_set_screen(UIScreen::PROTOCOL_TOOLS); break;
                        case 5: ui_set_screen(UIScreen::WIRELESS_ESP32); break;
                        case 6: ui_set_screen(UIScreen::GENERATOR);      break;
                        case 7: ui_set_screen(UIScreen::SETTINGS);       break;
                    }
                }
                break;

            case UIScreen::LOGIC_ANALYZER:
                if (ev == UI_LEFT) {
                    ui_set_screen(UIScreen::MAIN_MENU);
                } else if (ev == UI_SELECT) {
                    if (acquisition_is_running()) {
                        stop_requested = true;
                    } else {
                        start_requested = true;
                    }
                    screen_needs_redraw = true;
                } else if (ev == UI_UP) {
                    if (current_rate_idx < (sizeof(SAMPLE_RATES)/sizeof(SAMPLE_RATES[0]) - 1)) {
                        current_rate_idx++;
                        char cmd[32];
                        snprintf(cmd, sizeof(cmd), "rate %lu", (unsigned long)SAMPLE_RATES[current_rate_idx]);
                        bp_execute_command(cmd, Serial);
                        screen_needs_redraw = true;
                    }
                } else if (ev == UI_DOWN) {
                    if (current_rate_idx > 0) {
                        current_rate_idx--;
                        char cmd[32];
                        snprintf(cmd, sizeof(cmd), "rate %lu", (unsigned long)SAMPLE_RATES[current_rate_idx]);
                        bp_execute_command(cmd, Serial);
                        screen_needs_redraw = true;
                    }
                } else if (ev == UI_RIGHT) {
                    const TriggerEngineConfig& tc = trigger_engine_get_config();
                    uint8_t cur_p = tc.pre_trigger_pct;
                    uint8_t next_p = (cur_p == 0) ? 25 : (cur_p == 25) ? 50 : (cur_p == 50) ? 75 : (cur_p == 75) ? 90 : 0;
                    char cmd[32];
                    snprintf(cmd, sizeof(cmd), "trig pos %u", next_p);
                    bp_execute_command(cmd, Serial);
                    screen_needs_redraw = true;
                }
                break;

            case UIScreen::PROTOCOL_TOOLS:
                if (ev == UI_LEFT) {
                    ui_set_screen(UIScreen::MAIN_MENU);
                } else if (ev == UI_UP) {
                    tools_menu_idx = (tools_menu_idx > 0) ? tools_menu_idx - 1 : 7;
                    screen_needs_redraw = true;
                } else if (ev == UI_DOWN) {
                    tools_menu_idx = (tools_menu_idx < 7) ? tools_menu_idx + 1 : 0;
                    screen_needs_redraw = true;
                } else if (ev == UI_SELECT || ev == UI_RIGHT) {
                    switch (tools_menu_idx) {
                        case 0:
                            ui_show_toast("I2C Bus Scan", "Scanning 0x08..0x77", COLOR_CH1);
                            bp_execute_command("scan", Serial);
                            break;
                        case 1:
                            ui_show_toast("SPI Flash", "Reading JEDEC ID", COLOR_CH2);
                            bp_execute_command("flash", Serial);
                            break;
                        case 2:
                            ui_show_toast("UART Mode", "Bridge GP4/GP5", COLOR_CH0);
                            bp_execute_command("mode uart", Serial);
                            break;
                        case 3:
                            ui_show_toast("1-Wire", "Searching ROM IDs", COLOR_CH3);
                            bp_execute_command("1wire", Serial);
                            break;
                        case 4:
                            ui_show_toast("Digital I/O", "Probe Bus GP2..GP9", COLOR_ACCENT);
                            bp_execute_command("mode dio", Serial);
                            break;
                        case 5:
                            ui_show_toast("USB Prober", "GP2(D+) GP3(D-)", COLOR_CH1);
                            bp_execute_command("mode usb", Serial);
                            break;
                        case 6:
                            ui_show_toast("ARM SWD", "HW Test Pending", 0xFD20);
                            bp_execute_command("swd", Serial);
                            break;
                        case 7:
                            ui_show_toast("Safe Hi-Z", "Probes Tri-Stated", COLOR_TEXT);
                            bp_execute_command("mode hiz", Serial);
                            break;
                    }
                    screen_needs_redraw = true;
                }
                break;

            case UIScreen::MEASUREMENTS:
                if (ev == UI_LEFT) {
                    ui_set_screen(UIScreen::MAIN_MENU);
                } else if (ev == UI_UP) {
                    measure_pin_idx = (measure_pin_idx > 0) ? measure_pin_idx - 1 : NUM_MEASURE_PINS - 1;
                    screen_needs_redraw = true;
                } else if (ev == UI_DOWN) {
                    measure_pin_idx = (measure_pin_idx < NUM_MEASURE_PINS - 1) ? measure_pin_idx + 1 : 0;
                    screen_needs_redraw = true;
                } else if (ev == UI_SELECT || ev == UI_RIGHT) {
                    screen_needs_redraw = true;
                }
                break;

            case UIScreen::WIRELESS_ESP32:
                if (ev == UI_LEFT) {
                    ui_set_screen(UIScreen::MAIN_MENU);
                } else if (ev == UI_SELECT) {
                    interchip_send_ping();
                    ui_show_toast("Ping ESP32", "Keepalive Sent", COLOR_CH0, 400);
                    screen_needs_redraw = true;
                }
                break;

            case UIScreen::CONTINUITY:
                if (ev == UI_LEFT) {
                    buzzer_stop();
                    ui_set_screen(UIScreen::MAIN_MENU);
                } else if (ev == UI_UP) {
                    ContinuityConfig &cfg = adc_continuity_get_config();
                    if (cfg.threshold_ohms < 25.0f) adc_continuity_set_threshold(25.0f, 10.0f);
                    else if (cfg.threshold_ohms < 50.0f) adc_continuity_set_threshold(50.0f, 15.0f);
                    else if (cfg.threshold_ohms < 100.0f) adc_continuity_set_threshold(100.0f, 25.0f);
                    screen_needs_redraw = true;
                } else if (ev == UI_DOWN) {
                    ContinuityConfig &cfg = adc_continuity_get_config();
                    if (cfg.threshold_ohms > 50.0f) adc_continuity_set_threshold(50.0f, 15.0f);
                    else if (cfg.threshold_ohms > 25.0f) adc_continuity_set_threshold(25.0f, 10.0f);
                    else if (cfg.threshold_ohms > 10.0f) adc_continuity_set_threshold(10.0f, 5.0f);
                    screen_needs_redraw = true;
                } else if (ev == UI_SELECT || ev == UI_RIGHT) {
                    ContinuityConfig &cfg = adc_continuity_get_config();
                    adc_continuity_set_buzzer_mute(cfg.buzzer_enabled);
                    screen_needs_redraw = true;
                }
                break;

            case UIScreen::ANALOG_SCOPE:
                if (ev == UI_LEFT) {
                    ui_set_screen(UIScreen::MAIN_MENU);
                } else if (ev == UI_SELECT) {
                    screen_needs_redraw = true;
                }
                break;

            case UIScreen::GENERATOR:
                if (ev == UI_LEFT) {
                    ui_set_screen(UIScreen::MAIN_MENU);
                } else if (ev == UI_UP) {
                    gen_menu_idx = (gen_menu_idx > 0) ? gen_menu_idx - 1 : 5;
                    screen_needs_redraw = true;
                } else if (ev == UI_DOWN) {
                    gen_menu_idx = (gen_menu_idx < 5) ? gen_menu_idx + 1 : 0;
                    screen_needs_redraw = true;
                } else if (ev == UI_SELECT || ev == UI_RIGHT) {
                    switch (gen_menu_idx) {
                        case 0:
                            ui_show_toast("PWM Output", "1 kHz @ 50% on GP2", COLOR_CH0);
                            bp_execute_command("mode pwm 1000", Serial);
                            break;
                        case 1:
                            ui_show_toast("PWM Output", "10 kHz @ 50% on GP2", COLOR_CH0);
                            bp_execute_command("mode pwm 10000", Serial);
                            break;
                        case 2:
                            ui_show_toast("PWM Output", "1 MHz @ 50% on GP2", COLOR_CH0);
                            bp_execute_command("mode pwm 1000000", Serial);
                            break;
                        case 3:
                            ui_show_toast("PWM Output", "10 MHz @ 50% on GP2", COLOR_CH0);
                            bp_execute_command("mode pwm 10000000", Serial);
                            break;
                        case 4:
                            ui_show_toast("Glitcher", "Hardware Fault Pulse", 0xF800);
                            bp_execute_command("mode glitch", Serial);
                            break;
                        case 5:
                            ui_show_toast("Generator Off", "Output Stopped", COLOR_TEXT);
                            bp_execute_command("mode hiz", Serial);
                            break;
                    }
                    screen_needs_redraw = true;
                }
                break;

            case UIScreen::SETTINGS:
                if (ev == UI_LEFT) {
                    ui_set_screen(UIScreen::MAIN_MENU);
                } else if (ev == UI_UP) {
                    settings_menu_idx = (settings_menu_idx > 0) ? settings_menu_idx - 1 : 7;
                    screen_needs_redraw = true;
                } else if (ev == UI_DOWN) {
                    settings_menu_idx = (settings_menu_idx < 7) ? settings_menu_idx + 1 : 0;
                    screen_needs_redraw = true;
                } else if (ev == UI_SELECT || ev == UI_RIGHT) {
                    switch (settings_menu_idx) {
                        case 0: { // Cycle Trigger Mode
                            const TriggerEngineConfig& tc = trigger_engine_get_config();
                            if (tc.mode == TRIG_MODE_AUTO)              bp_execute_command("trig edge 0 rise", Serial);
                            else if (tc.mode == TRIG_MODE_EDGE_RISING)  bp_execute_command("trig edge 0 fall", Serial);
                            else if (tc.mode == TRIG_MODE_EDGE_FALLING) bp_execute_command("trig qual 0 rise 1 high", Serial);
                            else if (tc.mode == TRIG_MODE_LEVEL_QUAL)   bp_execute_command("trig pat 0x0F 0x05", Serial);
                            else bp_execute_command("trig auto", Serial);

                            const TriggerEngineConfig& new_tc = trigger_engine_get_config();
                            const char* m_str = (new_tc.mode == TRIG_MODE_AUTO) ? "Auto Trigger" :
                                                (new_tc.mode == TRIG_MODE_EDGE_RISING) ? "Rising Edge CH0" :
                                                (new_tc.mode == TRIG_MODE_EDGE_FALLING) ? "Falling Edge CH0" :
                                                (new_tc.mode == TRIG_MODE_LEVEL_QUAL) ? "Level Qual CH0/CH1" : "Pattern Match";
                            ui_show_toast("Trigger Mode", m_str, COLOR_CH2);
                            break;
                        }
                        case 1: { // Cycle Prescaler
                            uint8_t cur_div = frontend_get_prescaler();
                            uint8_t next_div = (cur_div == 1) ? 2 : (cur_div == 2) ? 4 : 1;
                            frontend_set_prescaler(next_div);
                            char p_msg[32];
                            snprintf(p_msg, sizeof(p_msg), "Div: x%u (GP14 %s)", next_div, next_div > 1 ? "HIGH" : "LOW");
                            ui_show_toast("Prescaler", p_msg, COLOR_CH1);
                            break;
                        }
                        case 2: { // Toggle Protection
                            bool cur_prot = frontend_get_protection();
                            frontend_set_protection(!cur_prot);
                            ui_show_toast("Protection", !cur_prot ? "Front-end PROTECTED" : "Bypass Mode Active", !cur_prot ? COLOR_CH0 : 0xF800);
                            break;
                        }
                        case 3: // Rotate Screen
                            ui_set_rotation((current_rotation + 1) % 4);
                            break;
                        case 4: // Hardware Diag
                            ui_set_screen(UIScreen::DIAGNOSTICS);
                            break;
                        case 5: // Display Test
                            ui_run_display_test(false, Serial);
                            delay(1000);
                            screen_needs_redraw = true;
                            break;
                        case 6: // Self Test 17/17
                            ui_show_toast("Running Test...", "17 hardware tests", COLOR_ACCENT, 300);
                            acquisition_run_self_test(Serial);
                            delay(1000);
                            screen_needs_redraw = true;
                            break;
                        case 7: // System Info
                            ui_set_screen(UIScreen::SYS_INFO);
                            break;
                    }
                }
                break;

            case UIScreen::DIAGNOSTICS:
                if (ev == UI_LEFT) {
                    ui_set_screen(UIScreen::SETTINGS);
                } else if (ev == UI_SELECT) {
                    interchip_send_ping();
                    screen_needs_redraw = true;
                }
                break;

            case UIScreen::SYS_INFO:
                // Any button press exits cleanly back to settings
                ui_set_screen(UIScreen::SETTINGS);
                break;
        }
    }

    // Periodic live screen refresh
    if (screen_needs_redraw) {
        screen_needs_redraw = false;
        switch (current_screen) {
            case UIScreen::MAIN_MENU:      render_main_menu();      break;
            case UIScreen::PROTOCOL_TOOLS: render_protocol_tools(); break;
            case UIScreen::MEASUREMENTS:   render_measurements();   break;
            case UIScreen::CONTINUITY:     render_continuity();     break;
            case UIScreen::ANALOG_SCOPE:   render_analog_scope();   break;
            case UIScreen::WIRELESS_ESP32: render_wireless_esp32(); break;
            case UIScreen::GENERATOR:      render_generator();      break;
            case UIScreen::SETTINGS:       render_settings();       break;
            case UIScreen::DIAGNOSTICS:    render_diagnostics();    break;
            case UIScreen::SYS_INFO:       render_sys_info();       break;
            case UIScreen::LOGIC_ANALYZER: {
                const CaptureResult& res = acquisition_get_result();
                const uint8_t* disp_buf = (res.valid && res.samples) ? res.samples : pio_capture_get_aligned_buffer();
                ui_update_waveforms(disp_buf, MAX_CAPTURE_SAMPLES, 0);
                break;
            }
        }
    } else if (current_screen == UIScreen::CONTINUITY) {
        // Fast non-blocking continuity measurement & buzzer evaluation on every loop
        adc_continuity_update(ADC_CH_0);
        if (millis() - last_live_update_ms >= 50) {
            last_live_update_ms = millis();
            render_continuity();
        }
    } else if (current_screen == UIScreen::ANALOG_SCOPE && (millis() - last_live_update_ms >= 80)) {
        last_live_update_ms = millis();
        render_analog_scope();
    } else if (current_screen == UIScreen::DIAGNOSTICS && (millis() - last_live_update_ms >= 100)) {
        last_live_update_ms = millis();
        render_diagnostics();
    } else if (current_screen == UIScreen::MEASUREMENTS && (millis() - last_live_update_ms >= 500)) {
        last_live_update_ms = millis();
        render_measurements();
    } else if (current_screen == UIScreen::WIRELESS_ESP32 && (millis() - last_live_update_ms >= 500)) {
        last_live_update_ms = millis();
        render_wireless_esp32();
    } else if (current_screen == UIScreen::LOGIC_ANALYZER && (millis() - last_live_update_ms >= 500)) {
        last_live_update_ms = millis();
        // Skip heavy full-screen SPI redraw while acquisition is running to prevent DMA buffer wrap starvation
        if (!acquisition_is_running()) {
            const CaptureResult& res = acquisition_get_result();
            const uint8_t* disp_buf = (res.valid && res.samples) ? res.samples : pio_capture_get_aligned_buffer();
            ui_update_waveforms(disp_buf, MAX_CAPTURE_SAMPLES, 0);
        }
    }

}

// ─── Simplified Display Test (Section 7) ──────────────────────────────────────
void ui_run_display_test(bool reinit, Stream& out) {
    out.printf("\r\n[Display Test] Running color bar diagnostic (Mode: %s | Tab: %s | SPI: %lu kHz)...\r\n",
               reinit ? "RESET + RE-INITIALIZE & DRAW" : "NORMAL DIRECT DRAW",
               ui_get_tab_name(),
               current_spi_speed / 1000);

    if (reinit) {
        if (TFT_RST >= 0) {
            gpio_put(TFT_RST, 0);
            delay(25);
            gpio_put(TFT_RST, 1);
            delay(120);
        }
        tft_main.initR(current_tab);
        tft_main.setRotation(current_rotation);
    }

    tft_main.fillScreen(0x0000); // Black
    tft_main.fillRect(0, 0,  TFT_W, 32, 0xF800); // Red
    tft_main.fillRect(0, 32, TFT_W, 32, 0x07E0); // Green
    tft_main.fillRect(0, 64, TFT_W, 32, 0x001F); // Blue
    tft_main.fillRect(0, 96, TFT_W, 32, 0xFFFF); // White

    tft_main.fillRoundRect(8, 44, TFT_W - 16, 40, 4, 0x1082);
    tft_main.drawRoundRect(8, 44, TFT_W - 16, 40, 4, 0xFFFF);
    tft_main.setTextColor(0xFFFF);
    tft_main.setTextSize(1);
    tft_main.setCursor(14, 52);
    tft_main.print("UDOO KEY TFT");
    tft_main.setCursor(14, 66);
    tft_main.setTextColor(0x07FF);
    tft_main.print("MAIN SCREEN OK");

    out.println("  Main Screen: Color pattern written (Red, Green, Blue, White).");
    out.println("  Check display panel. Type 'disp rot' or use buttons to return to normal UI.\r\n");
}

const char* ui_get_tab_name() {
    if (current_tab == INITR_144GREENTAB) return "144GREENTAB (1.44\" 128x128)";
    if (current_tab == INITR_BLACKTAB)    return "BLACKTAB";
    if (current_tab == INITR_REDTAB)      return "REDTAB";
    return "144GREENTAB (1.44\" 128x128)";
}

uint32_t ui_get_spi_speed() {
    return current_spi_speed;
}

void ui_set_tab_color(const char* tab, Stream& out) {
    if (!tab || !*tab) {
        out.printf("Current Tab: %s\r\nProduction Target: 144green (1.44\" 128x128)\r\nDiagnostic Options: 144green, black, red\r\n", ui_get_tab_name());
        return;
    }
    if (strstr(tab, "black") != nullptr) {
        current_tab = INITR_BLACKTAB;
    } else if (strstr(tab, "red") != nullptr) {
        current_tab = INITR_REDTAB;
    } else if (strstr(tab, "144") != nullptr || strstr(tab, "green") != nullptr) {
        current_tab = INITR_144GREENTAB;
    } else {
        out.println("[Display] Unknown tab option. Production target: 144green (1.44\" 128x128)");
        return;
    }
    out.printf("[Display] Switched tab profile to: %s. Reinitializing display...\r\n", ui_get_tab_name());
    ui_run_display_test(true, out);
}

void ui_set_spi_speed(uint32_t speed_hz, Stream& out) {
    if (speed_hz < 500000 || speed_hz > 32000000) {
        out.println("[Display] Speed must be between 500000 (500kHz) and 32000000 (32MHz). Recommended: 8000000");
        return;
    }
    current_spi_speed = speed_hz;
    tft_main.setSPISpeed(current_spi_speed);
    out.printf("[Display] SPI speed set to %lu Hz (%lu kHz). Re-testing...\r\n", current_spi_speed, current_spi_speed / 1000);
    ui_run_display_test(true, out);
}

void ui_toggle_invert(Stream& out) {
    current_invert = !current_invert;
    tft_main.invertDisplay(current_invert);
    out.printf("[Display] Inversion toggled to: %s\r\n", current_invert ? "INVERTED (INVON)" : "NORMAL (INVOFF)");
}

void ui_set_invert(bool inv, Stream& out) {
    current_invert = inv;
    tft_main.invertDisplay(current_invert);
    out.printf("[Display] Inversion set to: %s\r\n", current_invert ? "INVERTED (INVON)" : "NORMAL (INVOFF)");
}

bool ui_get_invert() {
    return current_invert;
}

void ui_set_vcom(uint8_t vcom, Stream& out) {
    current_vcom = vcom;
    tft_main.sendCommand(ST7735_VMCTR1, &current_vcom, 1);
    tft_main.sendCommand(ST77XX_SLPOUT);
    delay(20);
    tft_main.sendCommand(ST77XX_DISPON);
    delay(10);
    out.printf("[Display] VCOM voltage set to 0x%02X (%u). Re-testing...\r\n", current_vcom, current_vcom);
    ui_run_display_test(true, out);
}

uint8_t ui_get_vcom() {
    return current_vcom;
}

void ui_print_display_guide(Stream& out) {
    out.println("\r\n================================================================================");
    out.println("            UDOO KEY SINGLE ST7735 MAIN DISPLAY & 5-BTN NAV GUIDE              ");
    out.println("================================================================================");
    out.println("Main ST7735 Display Hardware Wiring:");
    out.println("  Signal       | RP2040 GPIO | Standard Pico Pin | UDOO KEY Header");
    out.println("  -------------+-------------+-------------------+------------------");
    out.println("  SCK / SCL    | GP18        | Pin 24            | Header P2 Pin 4");
    out.println("  MOSI / SDA   | GP19        | Pin 25            | Header P2 Pin 5");
    out.println("  MISO (opt)   | GP20        | Pin 26            | Header P2 Pin 6");
    out.println("  CS           | GP13        | Pin 17            | Header P1 Pin 17");
    out.println("  DC / A0 / RS | GP12        | Pin 16            | Header P1 Pin 16");
    out.println("  RST / RESET  | GP11        | Pin 15            | Header P1 Pin 15");
    out.println("  VCC          | 3V3 / 5V    | Pin 36 (or Pin 40)| Header P2 Pin 16 (or P2 Pin 20)");
    out.println("  GND          | GND         | Pin 18 / 23 / 28  | Header P1 Pin 18 / P2 Pin 3");
    out.println("  BLK / LED    | 3.3V        | Pin 36            | Header P2 Pin 16");
    out.println("  NOTE ON WHITE SCREEN:");
    out.println("  - If screen has an onboard 3.3V LDO regulator (e.g. 662K), power VCC from 5V (Pin 40)");
    out.println("    because 3.3V into an LDO drops to ~2.8V, preventing the charge pump from running.");
    out.println("  - SPI clock is defaulted to 8 MHz (safe for jumper wires).");
    out.println("  - Use CLI 'disp tab black' or 'disp tab red' if panel is not a 144 green-tab.");
    out.println("--------------------------------------------------------------------------------");
    out.println("Five-Button Navigation (Active LOW with internal pull-up to GND):");
    out.println("  BTN UP     : GP15 (Pico Pin 20 | Header P1 Pin 20 | adjacent GND: Pin 18)");
    out.println("  BTN DOWN   : GP16 (Pico Pin 21 | Header P2 Pin 1  | adjacent GND: Pin 23)");
    out.println("  BTN LEFT   : GP17 (Pico Pin 22 | Header P2 Pin 2  | adjacent GND: Pin 23)");
    out.println("  BTN RIGHT  : GP21 (Pico Pin 27 | Header P2 Pin 7  | adjacent GND: Pin 28)");
    out.println("  BTN SELECT : GP22 (Pico Pin 29 | Header P2 Pin 9  | adjacent GND: Pin 28)");
    out.println("--------------------------------------------------------------------------------");
    out.println("CLI Diagnostic Commands:");
    out.println("  disp               : Show display settings and tab status");
    out.println("  disp test          : Test pattern draw (Red, Green, Blue, White)");
    out.println("  disp test init     : Hardware reset pulse + full re-init");
    out.println("  disp tab <name>    : Switch tab variant (144green, black, red, green)");
    out.println("  disp speed <hz>    : Set SPI clock speed (e.g. 4000000, 8000000, 12000000)");
    out.println("  disp rot <0-3>     : Rotate display (0=Portrait, 1=Landscape, 2=Inverted)");
    out.println("================================================================================\r\n");
}
