#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <Arduino.h>

// Send array of RGB colors to WS2812 / NeoPixel strip
void ws2812_show(uint8_t pin, const uint32_t* colors, uint16_t num_leds);

// Set single LED color
void ws2812_set_color(uint8_t pin, uint8_t r, uint8_t g, uint8_t b);

// Display rainbow test pattern on N LEDs
void ws2812_show_rainbow(uint8_t pin, uint16_t num_leds, Stream& out);

// Turn off N LEDs on pin
void ws2812_turn_off(uint8_t pin, uint16_t num_leds);

#endif // WS2812_DRIVER_H
