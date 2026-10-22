#ifndef SWD_PROBE_H
#define SWD_PROBE_H

#include <Arduino.h>

struct SwdTargetInfo {
    uint32_t dpidr;
    uint32_t ap_idr;
    const char* part_name;
    const char* designer_name;
    uint8_t  revision;
    bool     target_found;
};

// Probe ARM target via SWD on specified pins
SwdTargetInfo swd_probe_target(uint8_t swclk_pin = 18, uint8_t swdio_pin = 19);

// Print formatted target interrogation result
void swd_print_info(uint8_t swclk_pin, uint8_t swdio_pin, Stream& out);

#endif // SWD_PROBE_H
