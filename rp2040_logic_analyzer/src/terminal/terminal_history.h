#ifndef TERMINAL_HISTORY_H
#define TERMINAL_HISTORY_H

#include <stddef.h>
#include <stdint.h>

#define TERMINAL_HISTORY_MAX_LINES 8
#define TERMINAL_HISTORY_LINE_LEN  128

#ifdef __cplusplus
extern "C" {
#endif

void        terminal_history_init();
void        terminal_history_add(const char* line);
const char* terminal_history_get_prev();
const char* terminal_history_get_next();
void        terminal_history_reset_nav();
void        terminal_history_clear();

#ifdef __cplusplus
}
#endif

#endif // TERMINAL_HISTORY_H
