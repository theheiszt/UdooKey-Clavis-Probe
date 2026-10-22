#include "macro_engine.h"
#include "bus_pirate_core.h"

static MacroEntry s_macros[MAX_MACROS];

void macro_init() {
    memset(s_macros, 0, sizeof(s_macros));

    // Preset standard Bus Pirate macros
    strncpy(s_macros[0].name, "I2C Scan", sizeof(s_macros[0].name));
    strncpy(s_macros[0].command, "scan", sizeof(s_macros[0].command));

    strncpy(s_macros[1].name, "I2C Read EEPROM", sizeof(s_macros[1].name));
    strncpy(s_macros[1].command, "[0x50 0x00][0x51 r:16]", sizeof(s_macros[1].command));

    strncpy(s_macros[2].name, "SPI JEDEC ID", sizeof(s_macros[2].name));
    strncpy(s_macros[2].command, "flash probe", sizeof(s_macros[2].command));

    strncpy(s_macros[3].name, "SPI SFDP Read", sizeof(s_macros[3].name));
    strncpy(s_macros[3].command, "flash sfdp", sizeof(s_macros[3].command));

    strncpy(s_macros[4].name, "1-Wire DS18B20", sizeof(s_macros[4].name));
    strncpy(s_macros[4].command, "1wire temp", sizeof(s_macros[4].command));

    strncpy(s_macros[5].name, "ARM SWD ID", sizeof(s_macros[5].name));
    strncpy(s_macros[5].command, "swd id", sizeof(s_macros[5].command));

    strncpy(s_macros[6].name, "AVR Signature", sizeof(s_macros[6].name));
    strncpy(s_macros[6].command, "avr sig", sizeof(s_macros[6].command));

    strncpy(s_macros[7].name, "Freq Probe Ch2", sizeof(s_macros[7].name));
    strncpy(s_macros[7].command, "f 2", sizeof(s_macros[7].command));

    strncpy(s_macros[8].name, "PWM 1MHz Clock", sizeof(s_macros[8].name));
    strncpy(s_macros[8].command, "g 1000000 50", sizeof(s_macros[8].command));

    strncpy(s_macros[9].name, "ADC / Voltages", sizeof(s_macros[9].name));
    strncpy(s_macros[9].command, "v", sizeof(s_macros[9].command));

    strncpy(s_macros[10].name, "Glitch Injection", sizeof(s_macros[10].name));
    strncpy(s_macros[10].command, "glitch 100 500", sizeof(s_macros[10].command));

    strncpy(s_macros[11].name, "Logic Capture", sizeof(s_macros[11].name));
    strncpy(s_macros[11].command, "la", sizeof(s_macros[11].command));
}

void macro_list(Stream& port) {
    port.println("\r\n=== Stored Bus Pirate Macros ===");
    for (uint8_t i = 0; i < MAX_MACROS; i++) {
        if (s_macros[i].command[0] != '\0') {
            port.printf("  Macro (%2d) [%-16s]: %s\r\n", i + 1, s_macros[i].name, s_macros[i].command);
        }
    }
    port.println("Use: macro <id> to execute, macro set <id> <name> <cmd> to edit.\r\n");
}

bool macro_run(uint8_t id, Stream& port) {
    if (id < 1 || id > MAX_MACROS) {
        port.printf("Invalid macro ID %d. Range: 1..%d\r\n", id, MAX_MACROS);
        return false;
    }
    uint8_t idx = id - 1;
    if (s_macros[idx].command[0] == '\0') {
        port.printf("Macro %d is empty.\r\n", id);
        return false;
    }

    port.printf("\r\n[EXEC MACRO %d: %s] -> %s\r\n", id, s_macros[idx].name, s_macros[idx].command);
    bp_execute_command(s_macros[idx].command, port);
    return true;
}

bool macro_set(uint8_t id, const char* name, const char* command) {
    if (id < 1 || id > MAX_MACROS || !command) return false;
    uint8_t idx = id - 1;
    if (name) {
        strncpy(s_macros[idx].name, name, sizeof(s_macros[idx].name) - 1);
        s_macros[idx].name[sizeof(s_macros[idx].name) - 1] = '\0';
    }
    strncpy(s_macros[idx].command, command, sizeof(s_macros[idx].command) - 1);
    s_macros[idx].command[sizeof(s_macros[idx].command) - 1] = '\0';
    return true;
}

const MacroEntry* macro_get(uint8_t id) {
    if (id < 1 || id > MAX_MACROS) return nullptr;
    return &s_macros[id - 1];
}
