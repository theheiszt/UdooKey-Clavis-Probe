#include "terminal_line_editor.h"
#include "terminal_transport.h"
#include "terminal_vt100.h"
#include "terminal_history.h"
#include <string.h>

static char s_prompt[32] = "HiZ> ";
static char s_buf[LINE_EDITOR_MAX_LEN];
static uint8_t s_len = 0;
static uint8_t s_pos = 0;

enum EscState {
    ESC_IDLE,
    ESC_SAW_ESC,
    ESC_SAW_BRACKET,
    ESC_PARAM
};

static EscState s_esc_state = ESC_IDLE;
static char s_esc_param = 0;

static void redraw_current_line() {
    vt100_clear_line();
    vt100_set_color(VT100_COLOR_PROMPT);
    terminal_transport_print(s_prompt);
    vt100_set_color(VT100_COLOR_RESET);
    terminal_transport_print(s_buf);

    // If cursor is not at end of line, move cursor left to s_pos
    if (s_pos < s_len) {
        vt100_cursor_left(s_len - s_pos);
    }
}

void line_editor_init(const char* prompt) {
    if (prompt) {
        strncpy(s_prompt, prompt, sizeof(s_prompt) - 1);
        s_prompt[sizeof(s_prompt) - 1] = '\0';
    }
    s_len = 0;
    s_pos = 0;
    s_buf[0] = '\0';
    s_esc_state = ESC_IDLE;
    terminal_history_init();
}

void line_editor_set_prompt(const char* prompt) {
    if (!prompt) return;
    strncpy(s_prompt, prompt, sizeof(s_prompt) - 1);
    s_prompt[sizeof(s_prompt) - 1] = '\0';
}

const char* line_editor_get_prompt() {
    return s_prompt;
}

void line_editor_reset() {
    s_len = 0;
    s_pos = 0;
    s_buf[0] = '\0';
    s_esc_state = ESC_IDLE;
    terminal_history_reset_nav();
    vt100_set_color(VT100_COLOR_PROMPT);
    terminal_transport_print(s_prompt);
    vt100_set_color(VT100_COLOR_RESET);
}

void line_editor_redraw() {
    redraw_current_line();
}

bool line_editor_feed(char c, char* out_line, size_t max_len) {
    // ─── VT100 Escape Sequence State Machine ──────────────────────────────────
    if (s_esc_state == ESC_SAW_ESC) {
        if (c == '[' || c == 'O') {
            s_esc_state = ESC_SAW_BRACKET;
            return false;
        } else {
            s_esc_state = ESC_IDLE;
            return false;
        }
    } else if (s_esc_state == ESC_SAW_BRACKET) {
        if (c >= '0' && c <= '9') {
            s_esc_param = c;
            s_esc_state = ESC_PARAM;
            return false;
        }
        s_esc_state = ESC_IDLE;

        switch (c) {
            case 'A': { // UP Arrow -> History Previous
                const char* prev = terminal_history_get_prev();
                if (prev) {
                    strncpy(s_buf, prev, sizeof(s_buf) - 1);
                    s_buf[sizeof(s_buf) - 1] = '\0';
                    s_len = strlen(s_buf);
                    s_pos = s_len;
                    redraw_current_line();
                }
                return false;
            }

            case 'B': { // DOWN Arrow -> History Next
                const char* next = terminal_history_get_next();
                if (next) {
                    strncpy(s_buf, next, sizeof(s_buf) - 1);
                    s_buf[sizeof(s_buf) - 1] = '\0';
                    s_len = strlen(s_buf);
                    s_pos = s_len;
                    redraw_current_line();
                }
                return false;
            }

            case 'C': { // RIGHT Arrow
                if (s_pos < s_len) {
                    s_pos++;
                    vt100_cursor_right(1);
                }
                return false;
            }

            case 'D': { // LEFT Arrow
                if (s_pos > 0) {
                    s_pos--;
                    vt100_cursor_left(1);
                }
                return false;
            }

            case 'H': { // HOME
                if (s_pos > 0) {
                    vt100_cursor_left(s_pos);
                    s_pos = 0;
                }
                return false;
            }

            case 'F': { // END
                if (s_pos < s_len) {
                    vt100_cursor_right(s_len - s_pos);
                    s_pos = s_len;
                }
                return false;
            }

            default:
                return false;
        }
    } else if (s_esc_state == ESC_PARAM) {
        s_esc_state = ESC_IDLE;
        if (c == '~') {
            if (s_esc_param == '3') { // DELETE key
                if (s_pos < s_len) {
                    memmove(&s_buf[s_pos], &s_buf[s_pos + 1], s_len - s_pos);
                    s_len--;
                    s_buf[s_len] = '\0';
                    redraw_current_line();
                }
            } else if (s_esc_param == '1' || s_esc_param == '7') { // HOME
                if (s_pos > 0) {
                    vt100_cursor_left(s_pos);
                    s_pos = 0;
                }
            } else if (s_esc_param == '4' || s_esc_param == '8') { // END
                if (s_pos < s_len) {
                    vt100_cursor_right(s_len - s_pos);
                    s_pos = s_len;
                }
            }
        }
        return false;
    }

    // ─── Normal Input Processing ──────────────────────────────────────────────
    if (c == '\033') { // ESC
        s_esc_state = ESC_SAW_ESC;
        return false;
    }

    static char s_prev_char = 0;
    if (c == '\n' && s_prev_char == '\r') {
        s_prev_char = 0;
        return false; // Swallow trailing \n of \r\n to prevent double prompt
    }
    s_prev_char = c;

    if (c == '\r' || c == '\n') { // ENTER
        terminal_transport_print("\r\n");
        if (out_line && max_len > 0) {
            strncpy(out_line, s_buf, max_len - 1);
            out_line[max_len - 1] = '\0';
        }
        if (s_len > 0) {
            terminal_history_add(s_buf);
        }
        s_len = 0;
        s_pos = 0;
        s_buf[0] = '\0';
        terminal_history_reset_nav();
        return true;
    }

    if (c == 0x08 || c == 0x7F) { // BACKSPACE
        if (s_pos > 0) {
            if (s_pos == s_len) {
                // At end of line: simple backspace-space-backspace
                s_pos--;
                s_len--;
                s_buf[s_len] = '\0';
                terminal_transport_print("\b \b");
            } else {
                // In middle of line: shift buffer and redraw
                memmove(&s_buf[s_pos - 1], &s_buf[s_pos], s_len - s_pos + 1);
                s_pos--;
                s_len--;
                redraw_current_line();
            }
        }
        return false;
    }

    if (c == 0x03) { // CTRL+C (Abort line)
        terminal_transport_print("^C\r\n");
        s_len = 0;
        s_pos = 0;
        s_buf[0] = '\0';
        terminal_history_reset_nav();
        vt100_set_color(VT100_COLOR_PROMPT);
        terminal_transport_print(s_prompt);
        vt100_set_color(VT100_COLOR_RESET);
        return false;
    }

    if (c == 0x0C) { // CTRL+L (Clear screen and redraw)
        vt100_clear_screen();
        redraw_current_line();
        return false;
    }

    // Ignore non-printable control characters
    if ((uint8_t)c < 0x20) {
        return false;
    }

    // Insert printable character into buffer
    if (s_len < LINE_EDITOR_MAX_LEN - 1) {
        if (s_pos == s_len) {
            s_buf[s_pos++] = c;
            s_len++;
            s_buf[s_len] = '\0';
            terminal_transport_write((uint8_t)c);
        } else {
            // Insert in middle
            memmove(&s_buf[s_pos + 1], &s_buf[s_pos], s_len - s_pos + 1);
            s_buf[s_pos++] = c;
            s_len++;
            redraw_current_line();
        }
    }

    return false;
}
