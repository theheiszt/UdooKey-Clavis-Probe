#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "pio_capture.h"
#include "button_nav.h"

// Production Display Geometry (Physical 1.44" 128x128 ST7735 Display Module)
constexpr int DISPLAY_WIDTH  = 128;
constexpr int DISPLAY_HEIGHT = 128;

#define TFT_W DISPLAY_WIDTH
#define TFT_H DISPLAY_HEIGHT

// Color definitions
#define COLOR_BG        0x0000   // Black background
#define COLOR_GRID      0x18E3   // Dark grid lines (#181818)
#define COLOR_CH0       0x07E0   // Green
#define COLOR_CH1       0x07FF   // Cyan
#define COLOR_CH2       0xFFE0   // Yellow
#define COLOR_CH3       0xFD20   // Orange
#define COLOR_CH4       0xF81F   // Magenta
#define COLOR_CH5       0x9E3F   // Purple
#define COLOR_CH6       0x041F   // Light Blue
#define COLOR_CH7       0xFFFF   // White
#define COLOR_TEXT      0xE79C   // Light grey text
#define COLOR_ACCENT    0x38BF   // Accent Cyan
#define COLOR_HEADER    0x1082   // Header dark grey / navy
#define COLOR_SELECT    0x0596   // Highlighted row green

// Single Main Display ST7735 instance (Bottom screen hardware)
extern Adafruit_ST7735 tft_main;

// Backward-compatibility aliases
#define tft_disp tft_main
#define tft_ctrl tft_main

// UI Screen modes
enum class UIScreen {
    MAIN_MENU,
    LOGIC_ANALYZER,
    PROTOCOL_TOOLS,
    MEASUREMENTS,
    CONTINUITY,
    ANALOG_SCOPE,
    WIRELESS_ESP32,
    GENERATOR,
    SETTINGS,
    DIAGNOSTICS,
    SYS_INFO,
    RESOURCE_CONFIRM
};


void ui_init();
void ui_set_rotation(uint8_t rot);
uint8_t ui_get_rotation();

void ui_select_display();
void ui_deselect_display();

void ui_task(bool& start_requested, bool& stop_requested);
void ui_set_screen(UIScreen screen);
UIScreen ui_get_screen();
void ui_show_toast(const char* title, const char* subtitle = nullptr, uint16_t color = 0x38BF, uint32_t duration_ms = 700);

void ui_update_waveforms(const uint8_t* buffer, uint32_t total_samples, uint32_t start_index);
void ui_notify_capture_complete();
void ui_run_display_test(bool reinit, Stream& out);
void ui_print_display_guide(Stream& out);
void ui_set_tab_color(const char* tab, Stream& out);
void ui_set_spi_speed(uint32_t speed_hz, Stream& out);
void ui_toggle_invert(Stream& out);
void ui_set_invert(bool inv, Stream& out);
bool ui_get_invert();
const char* ui_get_tab_name();
uint32_t ui_get_spi_speed();
void ui_set_vcom(uint8_t vcom, Stream& out);
uint8_t ui_get_vcom();

#endif // DISPLAY_UI_H
