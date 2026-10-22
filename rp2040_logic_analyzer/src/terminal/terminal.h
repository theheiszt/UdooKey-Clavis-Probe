#ifndef TERMINAL_H
#define TERMINAL_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void        terminal_init();
void        terminal_task();
void        terminal_print_banner();
void        terminal_print_prompt();
void        terminal_set_mode_name(const char* mode);
const char* terminal_get_mode_name();
void        terminal_execute_command(const char* line);

#ifdef __cplusplus
}
#endif

#endif // TERMINAL_H
