#include "terminal_history.h"
#include <string.h>

static char s_history[TERMINAL_HISTORY_MAX_LINES][TERMINAL_HISTORY_LINE_LEN];
static uint8_t s_count = 0;
static uint8_t s_head = 0;
static int8_t  s_nav_idx = -1;

void terminal_history_init() {
    terminal_history_clear();
}

void terminal_history_clear() {
    memset(s_history, 0, sizeof(s_history));
    s_count = 0;
    s_head = 0;
    s_nav_idx = -1;
}

void terminal_history_add(const char* line) {
    if (!line || !*line) return;

    // Skip duplicate if identical to previous entry
    if (s_count > 0) {
        uint8_t prev_slot = (s_head + TERMINAL_HISTORY_MAX_LINES - 1) % TERMINAL_HISTORY_MAX_LINES;
        if (strncmp(s_history[prev_slot], line, TERMINAL_HISTORY_LINE_LEN) == 0) {
            s_nav_idx = -1;
            return;
        }
    }

    strncpy(s_history[s_head], line, TERMINAL_HISTORY_LINE_LEN - 1);
    s_history[s_head][TERMINAL_HISTORY_LINE_LEN - 1] = '\0';

    s_head = (s_head + 1) % TERMINAL_HISTORY_MAX_LINES;
    if (s_count < TERMINAL_HISTORY_MAX_LINES) {
        s_count++;
    }
    s_nav_idx = -1;
}

const char* terminal_history_get_prev() {
    if (s_count == 0) return nullptr;

    if (s_nav_idx == -1) {
        s_nav_idx = (int8_t)s_count - 1;
    } else if (s_nav_idx > 0) {
        s_nav_idx--;
    }

    // Map s_nav_idx (0 = oldest, s_count-1 = newest) to circular buffer slot
    uint8_t start_slot = (s_head + TERMINAL_HISTORY_MAX_LINES - s_count) % TERMINAL_HISTORY_MAX_LINES;
    uint8_t slot = (start_slot + (uint8_t)s_nav_idx) % TERMINAL_HISTORY_MAX_LINES;
    return s_history[slot];
}

const char* terminal_history_get_next() {
    if (s_count == 0 || s_nav_idx == -1) return nullptr;

    if (s_nav_idx < (int8_t)s_count - 1) {
        s_nav_idx++;
        uint8_t start_slot = (s_head + TERMINAL_HISTORY_MAX_LINES - s_count) % TERMINAL_HISTORY_MAX_LINES;
        uint8_t slot = (start_slot + (uint8_t)s_nav_idx) % TERMINAL_HISTORY_MAX_LINES;
        return s_history[slot];
    } else {
        s_nav_idx = -1;
        return ""; // Return empty line when scrolled past newest entry
    }
}

void terminal_history_reset_nav() {
    s_nav_idx = -1;
}
