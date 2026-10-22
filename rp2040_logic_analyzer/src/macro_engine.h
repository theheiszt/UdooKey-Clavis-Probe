#ifndef MACRO_ENGINE_H
#define MACRO_ENGINE_H

#include <Arduino.h>

#define MAX_MACROS 16
#define MACRO_STR_LEN 64

struct MacroEntry {
    char name[24];
    char command[MACRO_STR_LEN];
};

void macro_init();
void macro_list(Stream& port);
bool macro_run(uint8_t id, Stream& port);
bool macro_set(uint8_t id, const char* name, const char* command);
const MacroEntry* macro_get(uint8_t id);

#endif // MACRO_ENGINE_H
