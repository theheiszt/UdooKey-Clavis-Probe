#pragma once

#include <Arduino.h>

namespace Esp32Core {

enum class DiagnosticStatus : uint8_t {
    PASS,
    FAIL,
    SKIP,
    NOT_PRESENT,
    NOT_TESTED
};

const char* diagStatusToString(DiagnosticStatus st);

struct DiagnosticResult {
    const char* testName;
    DiagnosticStatus status;
    char details[64];
};

class Diagnostics {
public:
    static void runSelfTest(Print& out, Stream* rp2040Stream = nullptr);
    static void dumpSystemInfo(Print& out);
};

} // namespace Esp32Core
