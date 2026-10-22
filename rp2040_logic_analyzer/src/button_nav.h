#ifndef BUTTON_NAV_H
#define BUTTON_NAV_H

#include <Arduino.h>

// Standard Five-Button Interface
enum Button {
    BTN_NONE   = 0,
    BTN_UP     = 1,
    BTN_DOWN   = 2,
    BTN_LEFT   = 3,
    BTN_RIGHT  = 4,
    BTN_SELECT = 5
};

// UI Events for decoupled menu state machine
enum UIEvent {
    UI_NONE   = 0,
    UI_UP     = 1,
    UI_DOWN   = 2,
    UI_LEFT   = 3,
    UI_RIGHT  = 4,
    UI_SELECT = 5
};

// Physical Button Pin Configuration
// Defaults to verified conflict-free pins freed by display/touch refactor
#ifndef PIN_BTN_UP
#define PIN_BTN_UP 15
#endif

#ifndef PIN_BTN_DOWN
#define PIN_BTN_DOWN 16
#endif

#ifndef PIN_BTN_LEFT
#define PIN_BTN_LEFT 17
#endif

#ifndef PIN_BTN_RIGHT
#define PIN_BTN_RIGHT 21
#endif

#ifndef PIN_BTN_SELECT
#define PIN_BTN_SELECT 22
#endif

void button_nav_init();
Button ui_get_button();
UIEvent button_nav_poll();
void button_nav_inject(Button btn);
bool button_nav_pins_configured();

#endif // BUTTON_NAV_H
