#include "terminal_vt100.h"
#include "terminal_transport.h"

static bool s_vt100_enabled = true;

void vt100_init() {
    s_vt100_enabled = true;
}

void vt100_set_enabled(bool enabled) {
    s_vt100_enabled = enabled;
}

bool vt100_is_enabled() {
    return s_vt100_enabled;
}

void vt100_set_color(TerminalColor color) {
    if (!s_vt100_enabled) return;

    switch (color) {
        case VT100_COLOR_RESET:
            terminal_transport_print("\033[0m");
            break;
        case VT100_COLOR_PROMPT:
            terminal_transport_print("\033[1;36m"); // Bold Cyan
            break;
        case VT100_COLOR_INFO:
            terminal_transport_print("\033[1;32m"); // Bold Green
            break;
        case VT100_COLOR_NOTICE:
            terminal_transport_print("\033[36m");   // Cyan
            break;
        case VT100_COLOR_WARNING:
            terminal_transport_print("\033[1;33m"); // Bold Yellow
            break;
        case VT100_COLOR_ERROR:
            terminal_transport_print("\033[1;31m"); // Bold Red
            break;
        case VT100_COLOR_VALUE:
            terminal_transport_print("\033[1;37m"); // Bright White
            break;
        case VT100_COLOR_DIM:
            terminal_transport_print("\033[2;37m"); // Dim
            break;
        default:
            terminal_transport_print("\033[0m");
            break;
    }
}

void vt100_clear_screen() {
    if (s_vt100_enabled) {
        terminal_transport_print("\033[2J\033[H");
    } else {
        terminal_transport_print("\r\n\r\n\r\n");
    }
}

void vt100_clear_line() {
    if (s_vt100_enabled) {
        terminal_transport_print("\033[2K\r");
    } else {
        terminal_transport_print("\r");
    }
}

void vt100_cursor_home() {
    if (s_vt100_enabled) {
        terminal_transport_print("\033[H");
    }
}

void vt100_cursor_left(uint16_t n) {
    if (!s_vt100_enabled || n == 0) return;
    if (n == 1) {
        terminal_transport_print("\033[D");
    } else {
        terminal_transport_printf("\033[%uD", n);
    }
}

void vt100_cursor_right(uint16_t n) {
    if (!s_vt100_enabled || n == 0) return;
    if (n == 1) {
        terminal_transport_print("\033[C");
    } else {
        terminal_transport_printf("\033[%uC", n);
    }
}

void vt100_cursor_save() {
    if (s_vt100_enabled) {
        terminal_transport_print("\033[s");
    }
}

void vt100_cursor_restore() {
    if (s_vt100_enabled) {
        terminal_transport_print("\033[u");
    }
}

void vt100_cursor_show(bool show) {
    if (s_vt100_enabled) {
        terminal_transport_print(show ? "\033[?25h" : "\033[?25l");
    }
}
