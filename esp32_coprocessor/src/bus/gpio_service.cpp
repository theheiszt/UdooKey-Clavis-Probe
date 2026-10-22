#include "gpio_service.h"
#include "../core/resource_manager.h"
#include <cstring>
#include <strings.h>

namespace Esp32Bus {

GpioService& GpioService::instance() {
    static GpioService s_instance;
    return s_instance;
}

void GpioService::listPins(Print& out) {
    out.println("\r\n=== ESP32 GPIO STATUS ===");
    out.println(" Pin   Mode       State   Owner          Notes");
    out.println(" ────  ─────────  ─────   ─────────────  ────────────────────");

    auto& rm = Esp32Core::ResourceManager::instance();
    for (uint8_t p = 0; p < 40; ++p) {
        if (p == 20 || p == 24 || (p >= 28 && p <= 31)) continue; // Non-existent pins

        const char* note = "";
        if (rm.isPinProtected(p)) {
            note = "SYSTEM RESERVED (Locked)";
        } else if (rm.isPinInputOnly(p)) {
            note = "INPUT ONLY (No output/pull)";
        }

        int state = digitalRead(p);
        const char* stateStr = (state == HIGH) ? "HIGH (1)" : "LOW  (0)";
        const char* ownerStr = Esp32Core::ownerToString(rm.getPinOwner(p));

        out.printf(" IO%-2u  %-9s  %-7s %-14s %s\r\n", p, "N/A", stateStr, ownerStr, note);
    }
    out.println("=========================");
}

bool GpioService::setPinMode(uint8_t pin, const char* modeStr, Print& out) {
    auto& rm = Esp32Core::ResourceManager::instance();
    if (rm.isPinProtected(pin)) {
        out.printf("Error: GPIO%u is a protected system pin and cannot be modified.\r\n", pin);
        return false;
    }

    if (rm.isPinInputOnly(pin) && (strcasecmp(modeStr, "output") == 0 || strcasecmp(modeStr, "pullup") == 0 || strcasecmp(modeStr, "pulldown") == 0)) {
        out.printf("Error: GPIO%u is an input-only pin without internal pull-ups or output drivers.\r\n", pin);
        return false;
    }

    auto err = rm.acquirePin(pin, Esp32Core::Owner::USER);
    if (err != Esp32Core::ResourceError::OK) {
        out.printf("Error: Failed to claim GPIO%u (%s)\r\n", pin, Esp32Core::resourceErrorToString(err));
        return false;
    }

    if (strcasecmp(modeStr, "input") == 0) {
        pinMode(pin, INPUT);
        out.printf("GPIO%u mode: INPUT\r\n", pin);
    } else if (strcasecmp(modeStr, "output") == 0) {
        pinMode(pin, OUTPUT);
        out.printf("GPIO%u mode: OUTPUT\r\n", pin);
    } else if (strcasecmp(modeStr, "pullup") == 0) {
        pinMode(pin, INPUT_PULLUP);
        out.printf("GPIO%u mode: INPUT_PULLUP\r\n", pin);
    } else if (strcasecmp(modeStr, "pulldown") == 0) {
        pinMode(pin, INPUT_PULLDOWN);
        out.printf("GPIO%u mode: INPUT_PULLDOWN\r\n", pin);
    } else {
        out.printf("Error: Unknown mode '%s'. Use input, output, pullup, or pulldown.\r\n", modeStr);
        return false;
    }
    return true;
}

bool GpioService::readPin(uint8_t pin, Print& out) {
    auto& rm = Esp32Core::ResourceManager::instance();
    if (rm.isPinProtected(pin)) {
        out.printf("Error: GPIO%u is a protected system pin.\r\n", pin);
        return false;
    }
    int val = digitalRead(pin);
    out.printf("GPIO%u state: %s (%d) | Owner: %s\r\n",
               pin, val ? "HIGH" : "LOW", val,
               Esp32Core::ownerToString(rm.getPinOwner(pin)));
    return true;
}

bool GpioService::writePin(uint8_t pin, uint8_t level, Print& out) {
    auto& rm = Esp32Core::ResourceManager::instance();
    if (rm.isPinProtected(pin) || rm.isPinInputOnly(pin)) {
        out.printf("Error: GPIO%u cannot be driven as an output.\r\n", pin);
        return false;
    }
    if (rm.getPinOwner(pin) != Esp32Core::Owner::USER) {
        auto err = rm.acquirePin(pin, Esp32Core::Owner::USER);
        if (err != Esp32Core::ResourceError::OK) {
            out.printf("Error: GPIO%u is claimed by %s\r\n", pin, Esp32Core::ownerToString(rm.getPinOwner(pin)));
            return false;
        }
        pinMode(pin, OUTPUT);
    }
    digitalWrite(pin, level ? HIGH : LOW);
    out.printf("GPIO%u set to %s (%d)\r\n", pin, level ? "HIGH" : "LOW", level ? 1 : 0);
    return true;
}

bool GpioService::togglePin(uint8_t pin, Print& out) {
    auto& rm = Esp32Core::ResourceManager::instance();
    if (rm.isPinProtected(pin) || rm.isPinInputOnly(pin)) {
        out.printf("Error: GPIO%u cannot be driven as an output.\r\n", pin);
        return false;
    }
    int cur = digitalRead(pin);
    int next = !cur;
    digitalWrite(pin, next ? HIGH : LOW);
    out.printf("GPIO%u toggled to %s (%d)\r\n", pin, next ? "HIGH" : "LOW", next);
    return true;
}

void GpioService::executeCommand(const char* args, Print& out) {
    while (*args && isspace(*args)) args++;
    if (!*args || strcasecmp(args, "list") == 0) {
        listPins(out);
        return;
    }

    char cmd[16] = {0};
    int n = 0;
    while (*args && !isspace(*args) && n < 15) {
        cmd[n++] = *args++;
    }
    while (*args && isspace(*args)) args++;

    if (strcasecmp(cmd, "mode") == 0) {
        int pin = -1;
        char mode[16] = {0};
        if (sscanf(args, "%d %15s", &pin, mode) == 2 && pin >= 0 && pin < 40) {
            setPinMode((uint8_t)pin, mode, out);
        } else {
            out.println("Usage: gpio mode <pin> <input|output|pullup|pulldown>");
        }
    } else if (strcasecmp(cmd, "read") == 0) {
        int pin = atoi(args);
        if (pin >= 0 && pin < 40) {
            readPin((uint8_t)pin, out);
        } else {
            out.println("Usage: gpio read <pin>");
        }
    } else if (strcasecmp(cmd, "write") == 0) {
        int pin = -1, val = -1;
        if (sscanf(args, "%d %d", &pin, &val) == 2 && pin >= 0 && pin < 40) {
            writePin((uint8_t)pin, val ? 1 : 0, out);
        } else {
            out.println("Usage: gpio write <pin> <0|1>");
        }
    } else if (strcasecmp(cmd, "toggle") == 0) {
        int pin = atoi(args);
        if (pin >= 0 && pin < 40) {
            togglePin((uint8_t)pin, out);
        } else {
            out.println("Usage: gpio toggle <pin>");
        }
    } else {
        out.println("Available commands: gpio list | gpio mode <p> <m> | gpio read <p> | gpio write <p> <v> | gpio toggle <p>");
    }
}

} // namespace Esp32Bus
