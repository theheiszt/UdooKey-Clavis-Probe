#ifndef TERMINAL_VT100_H
#define TERMINAL_VT100_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VT100_COLOR_RESET = 0,
    VT100_COLOR_PROMPT,
    VT100_COLOR_INFO,
    VT100_COLOR_NOTICE,
    VT100_COLOR_WARNING,
    VT100_COLOR_ERROR,
    VT100_COLOR_VALUE,
    VT100_COLOR_DIM
} TerminalColor;

void vt100_init();
void vt100_set_enabled(bool enabled);
bool vt100_is_enabled();

void vt100_set_color(TerminalColor color);
void vt100_clear_screen();
void vt100_clear_line();
void vt100_cursor_home();
void vt100_cursor_left(uint16_t n);
void vt100_cursor_right(uint16_t n);
void vt100_cursor_save();
void vt100_cursor_restore();
void vt100_cursor_show(bool show);

#ifdef __cplusplus
}
#endif

#endif // TERMINAL_VT100_H
