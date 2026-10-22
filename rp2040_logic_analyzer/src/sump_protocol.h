#ifndef SUMP_PROTOCOL_H
#define SUMP_PROTOCOL_H

#include <Arduino.h>
#include "pio_capture.h"

// SUMP (OpenBench Logic Sniffer) protocol command definitions
#define SUMP_CMD_RESET            0x00
#define SUMP_CMD_ARM              0x01
#define SUMP_CMD_ID               0x02
#define SUMP_CMD_META             0x04
#define SUMP_CMD_FINISH           0x05
#define SUMP_CMD_QUERY_INPUT      0x06

#define SUMP_CMD_SET_SAMPLE_RATE  0x80
#define SUMP_CMD_SET_COUNTS       0x81
#define SUMP_CMD_SET_FLAGS        0x82
#define SUMP_CMD_SET_BTRG0_MASK   0xC0
#define SUMP_CMD_SET_BTRG0_VALUE  0xC1
#define SUMP_CMD_SET_BTRG0_CONFIG 0xC2
#define SUMP_CMD_SET_BTRG1_MASK   0xC4
#define SUMP_CMD_SET_BTRG1_VALUE  0xC5
#define SUMP_CMD_SET_BTRG1_CONFIG 0xC6
#define SUMP_CMD_SET_BTRG2_MASK   0xC8
#define SUMP_CMD_SET_BTRG2_VALUE  0xC9
#define SUMP_CMD_SET_BTRG2_CONFIG 0xCA
#define SUMP_CMD_SET_BTRG3_MASK   0xCC
#define SUMP_CMD_SET_BTRG3_VALUE  0xCD
#define SUMP_CMD_SET_BTRG3_CONFIG 0xCE

#define SUMP_META_END             0x00
#define SUMP_META_NAME            0x01
#define SUMP_META_FPGA_VERSION    0x02
#define SUMP_META_CPU_VERSION     0x03
#define SUMP_META_SAMPLE_RAM      0x21
#define SUMP_META_SAMPLE_RATE     0x23
#define SUMP_META_PROTOCOL        0x24
#define SUMP_META_PROBES_B        0x40
#define SUMP_META_PROTOCOL_B      0x41

void sump_init();
bool sump_is_active();
void sump_set_active(bool active);
void sump_process_byte(uint8_t byte, Stream& port);
void sump_task(Stream& port);

#endif // SUMP_PROTOCOL_H
