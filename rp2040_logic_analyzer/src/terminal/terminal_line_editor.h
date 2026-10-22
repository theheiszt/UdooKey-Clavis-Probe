#ifndef TERMINAL_LINE_EDITOR_H
#define TERMINAL_LINE_EDITOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define LINE_EDITOR_MAX_LEN 128

#ifdef __cplusplus
extern "C" {
#endif

void line_editor_init(const char* prompt);
void line_editor_set_prompt(const char* prompt);
const char* line_editor_get_prompt();

// Feeds one byte from transport. Returns true if a full line was submitted (Enter pressed).
bool line_editor_feed(char c, char* out_line, size_t max_len);

// Clears line buffer and redisplays prompt
void line_editor_reset();

// Redraws prompt and current line content
void line_editor_redraw();

#ifdef __cplusplus
}
#endif

#endif // TERMINAL_LINE_EDITOR_H
