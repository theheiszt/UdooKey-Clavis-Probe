#include "button_nav.h"

// Compile-Time Hardware Collision Validation
#define ASSERT_VALID_BUTTON_PIN(pin, name) \
    static_assert((pin) != 0 && (pin) != 1, name " conflicts with ESP32 UART (GP0/GP1)"); \
    static_assert((pin) < 2 || (pin) > 9,   name " conflicts with Logic Probe Bus (GP2..GP9)"); \
    static_assert((pin) != 10,              name " conflicts with External Trigger (GP10)"); \
    static_assert((pin) != 11 && (pin) != 12 && (pin) != 13 && \
                  (pin) != 18 && (pin) != 19 && (pin) != 20, name " conflicts with Main TFT SPI Bus (GP11-13, 18-20)"); \
    static_assert((pin) != 14,              name " conflicts with Analog Prescaler (GP14)");

#if PIN_BTN_UP >= 0
ASSERT_VALID_BUTTON_PIN(PIN_BTN_UP, "PIN_BTN_UP")
#endif
#if PIN_BTN_DOWN >= 0
ASSERT_VALID_BUTTON_PIN(PIN_BTN_DOWN, "PIN_BTN_DOWN")
#endif
#if PIN_BTN_LEFT >= 0
ASSERT_VALID_BUTTON_PIN(PIN_BTN_LEFT, "PIN_BTN_LEFT")
#endif
#if PIN_BTN_RIGHT >= 0
ASSERT_VALID_BUTTON_PIN(PIN_BTN_RIGHT, "PIN_BTN_RIGHT")
#endif
#if PIN_BTN_SELECT >= 0
ASSERT_VALID_BUTTON_PIN(PIN_BTN_SELECT, "PIN_BTN_SELECT")
#endif

// Uniqueness assertions
#if PIN_BTN_UP >= 0 && PIN_BTN_DOWN >= 0
static_assert(PIN_BTN_UP != PIN_BTN_DOWN, "Duplicate pin assigned to UP and DOWN");
#endif
#if PIN_BTN_UP >= 0 && PIN_BTN_LEFT >= 0
static_assert(PIN_BTN_UP != PIN_BTN_LEFT, "Duplicate pin assigned to UP and LEFT");
#endif
#if PIN_BTN_UP >= 0 && PIN_BTN_RIGHT >= 0
static_assert(PIN_BTN_UP != PIN_BTN_RIGHT, "Duplicate pin assigned to UP and RIGHT");
#endif
#if PIN_BTN_UP >= 0 && PIN_BTN_SELECT >= 0
static_assert(PIN_BTN_UP != PIN_BTN_SELECT, "Duplicate pin assigned to UP and SELECT");
#endif
#if PIN_BTN_DOWN >= 0 && PIN_BTN_LEFT >= 0
static_assert(PIN_BTN_DOWN != PIN_BTN_LEFT, "Duplicate pin assigned to DOWN and LEFT");
#endif
#if PIN_BTN_DOWN >= 0 && PIN_BTN_RIGHT >= 0
static_assert(PIN_BTN_DOWN != PIN_BTN_RIGHT, "Duplicate pin assigned to DOWN and RIGHT");
#endif
#if PIN_BTN_DOWN >= 0 && PIN_BTN_SELECT >= 0
static_assert(PIN_BTN_DOWN != PIN_BTN_SELECT, "Duplicate pin assigned to DOWN and SELECT");
#endif
#if PIN_BTN_LEFT >= 0 && PIN_BTN_RIGHT >= 0
static_assert(PIN_BTN_LEFT != PIN_BTN_RIGHT, "Duplicate pin assigned to LEFT and RIGHT");
#endif
#if PIN_BTN_LEFT >= 0 && PIN_BTN_SELECT >= 0
static_assert(PIN_BTN_LEFT != PIN_BTN_SELECT, "Duplicate pin assigned to LEFT and SELECT");
#endif
#if PIN_BTN_RIGHT >= 0 && PIN_BTN_SELECT >= 0
static_assert(PIN_BTN_RIGHT != PIN_BTN_SELECT, "Duplicate pin assigned to RIGHT and SELECT");
#endif

struct ButtonState {
    int pin;
    Button btn;
    bool last_raw;
    bool debounced;
    uint32_t last_debounce_ms;
    uint32_t press_start_ms;
    uint32_t last_repeat_ms;
    bool hold_active;
};

static ButtonState buttons[5] = {
    { PIN_BTN_UP,     BTN_UP,     false, false, 0, 0, 0, false },
    { PIN_BTN_DOWN,   BTN_DOWN,   false, false, 0, 0, 0, false },
    { PIN_BTN_LEFT,   BTN_LEFT,   false, false, 0, 0, 0, false },
    { PIN_BTN_RIGHT,  BTN_RIGHT,  false, false, 0, 0, 0, false },
    { PIN_BTN_SELECT, BTN_SELECT, false, false, 0, 0, 0, false }
};

// Circular Event Queue
#define EVENT_QUEUE_SIZE 16
static UIEvent event_queue[EVENT_QUEUE_SIZE];
static uint8_t queue_head = 0;
static uint8_t queue_tail = 0;

static void queue_push(UIEvent ev) {
    uint8_t next = (queue_head + 1) % EVENT_QUEUE_SIZE;
    if (next != queue_tail) {
        event_queue[queue_head] = ev;
        queue_head = next;
    }
}

static UIEvent queue_pop() {
    if (queue_head == queue_tail) return UI_NONE;
    UIEvent ev = event_queue[queue_tail];
    queue_tail = (queue_tail + 1) % EVENT_QUEUE_SIZE;
    return ev;
}

void button_nav_init() {
    for (int i = 0; i < 5; i++) {
        if (buttons[i].pin >= 0) {
            pinMode(buttons[i].pin, INPUT_PULLUP);
            buttons[i].last_raw = (digitalRead(buttons[i].pin) == LOW);
            buttons[i].debounced = buttons[i].last_raw;
        } else {
            buttons[i].last_raw = false;
            buttons[i].debounced = false;
        }
        buttons[i].last_debounce_ms = 0;
        buttons[i].press_start_ms = 0;
        buttons[i].last_repeat_ms = 0;
        buttons[i].hold_active = false;
    }
    queue_head = 0;
    queue_tail = 0;
}

bool button_nav_pins_configured() {
    for (int i = 0; i < 5; i++) {
        if (buttons[i].pin >= 0) return true;
    }
    return false;
}

Button ui_get_button() {
    for (int i = 0; i < 5; i++) {
        if (buttons[i].debounced) return buttons[i].btn;
    }
    return BTN_NONE;
}

void button_nav_inject(Button btn) {
    switch (btn) {
        case BTN_UP:     queue_push(UI_UP);     break;
        case BTN_DOWN:   queue_push(UI_DOWN);   break;
        case BTN_LEFT:   queue_push(UI_LEFT);   break;
        case BTN_RIGHT:  queue_push(UI_RIGHT);  break;
        case BTN_SELECT: queue_push(UI_SELECT); break;
        default: break;
    }
}

UIEvent button_nav_poll() {
    uint32_t now = millis();
    const uint32_t DEBOUNCE_MS = 20;
    const uint32_t HOLD_DELAY_MS = 500;
    const uint32_t REPEAT_RATE_MS = 150;

    for (int i = 0; i < 5; i++) {
        if (buttons[i].pin < 0) continue;

        // Active LOW with internal pull-up
        bool raw = (digitalRead(buttons[i].pin) == LOW);

        if (raw != buttons[i].last_raw) {
            buttons[i].last_raw = raw;
            buttons[i].last_debounce_ms = now;
        }

        if ((now - buttons[i].last_debounce_ms) >= DEBOUNCE_MS) {
            if (raw != buttons[i].debounced) {
                buttons[i].debounced = raw;
                if (buttons[i].debounced) {
                    // Initial key press event
                    buttons[i].press_start_ms = now;
                    buttons[i].last_repeat_ms = now;
                    buttons[i].hold_active = false;
                    queue_push((UIEvent)buttons[i].btn);
                } else {
                    // Key released
                    buttons[i].hold_active = false;
                }
            } else if (buttons[i].debounced) {
                // Key held down -> check for auto-repeat
                if (!buttons[i].hold_active && ((now - buttons[i].press_start_ms) >= HOLD_DELAY_MS)) {
                    buttons[i].hold_active = true;
                    buttons[i].last_repeat_ms = now;
                    queue_push((UIEvent)buttons[i].btn);
                } else if (buttons[i].hold_active && ((now - buttons[i].last_repeat_ms) >= REPEAT_RATE_MS)) {
                    buttons[i].last_repeat_ms = now;
                    queue_push((UIEvent)buttons[i].btn);
                }
            }
        }
    }

    return queue_pop();
}
