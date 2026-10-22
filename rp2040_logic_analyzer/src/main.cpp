#include <Arduino.h>
#include "pio_capture.h"
#include "display_ui.h"
#include "serial_protocol.h"
#include "udoo_protocol.h"
#include "hardware_shells.h"
#include "bus_pirate_core.h"
#include "sump_protocol.h"
#include "macro_engine.h"
#include "pio_edge_analyzer.h"
#include "trigger_engine.h"
#include "spi_slave_transport.h"
#include "acquisition_api.h"
#include "interchip_link.h"
#include "terminal/terminal.h"
#include "debug_engine.h"
#include "instrument_engine.h"
#include "pinmux_manager.h"
#include "event_streamer.h"

#define ESP32_SERIAL Serial1

static LogicAnalyzerConfig current_config = {
    .sample_rate_hz   = 10000000,  // 10 MHz default
    .sample_count     = 8192,      // 8k samples default
    .trigger_channel = 0,
    .trigger_mode    = TRIG_IMMEDIATE,
    .enabled_channels= 0xFF
};

static unsigned long last_ui_refresh = 0;
static char usb_cmd_buffer[128];
static uint8_t usb_cmd_idx = 0;

void stream_buffer_to_esp32(const uint8_t* buffer, uint32_t length) {
    // Stream via InstrumentProtocol v1.0 chunks
    interchip_stream_capture(buffer, length);
}


void process_esp32_commands(bool& start_req, bool& stop_req) {
    if (ESP32_SERIAL.available() < (int)sizeof(PacketHeader)) return;

    // Check for modern InstrumentProtocol packet (0xAA55, little-endian 0x55)
    if (ESP32_SERIAL.peek() == (INSTRUMENT_PROTO_MAGIC & 0xFF)) {
        instrument_engine_process_stream(ESP32_SERIAL, start_req, stop_req);
        return;
    }

    if (ESP32_SERIAL.peek() != PROTOCOL_SYNC_BYTE) { ESP32_SERIAL.read(); return; }

    PacketHeader hdr;
    ESP32_SERIAL.readBytes((char*)&hdr, sizeof(hdr));

    if (hdr.type == PKT_LA_START) {
        start_req = true;
    } else if (hdr.type == PKT_LA_STOP) {
        stop_req = true;
    } else if (hdr.type == PKT_PIRATE_TERMINAL) {
        char cmd[128] = {0};
        uint16_t len = hdr.payload_len;
        if (len >= sizeof(cmd)) len = sizeof(cmd) - 1;
        ESP32_SERIAL.readBytes(cmd, len);
        cmd[len] = '\0';

        // Execute command and reply back to ESP32 over UART
        bp_execute_command(cmd, ESP32_SERIAL);
    } else if (hdr.type == PKT_PIRATE_SPI_FLASH) {
        bp_execute_command("flash probe", ESP32_SERIAL);
    } else if (hdr.type == PKT_PIRATE_JTAG_SWD) {
        bp_execute_command("swd", ESP32_SERIAL);
    } else if (hdr.type == PKT_PIRATE_I2C_SCAN) {
        bp_execute_command("scan", ESP32_SERIAL);
    } else if (hdr.type == PKT_PIRATE_IR_REMOTE) {
        bp_execute_command("ir nec 0x00FF 0x12", ESP32_SERIAL);
    }
}

void process_usb_cli(bool& start_requested, bool& stop_requested) {
    if (sump_is_active()) {
        while (Serial.available()) {
            uint8_t b = Serial.read();
            // Allow clean exit from SUMP mode back to serial terminal if user types Enter or ESC
            if (b == '\r' || b == '\n' || b == 0x1B) {
                sump_set_active(false);
                terminal_print_prompt();
                return;
            }
            sump_process_byte(b, Serial);
        }
        sump_task(Serial);
        return;
    }

    while (Serial.available()) {
        uint8_t peek_b = Serial.peek();
        // Automatic handshake detection for Sigrok / PulseView / SUMP clients
        if (peek_b == SUMP_CMD_RESET || peek_b == SUMP_CMD_ID || peek_b == SUMP_CMD_META) {
            sump_set_active(true);
            while (Serial.available()) {
                sump_process_byte(Serial.read(), Serial);
            }
            sump_task(Serial);
            return;
        }

        // Modern InstrumentProtocol v1.0 Magic: Binary packets (0xAA55, little-endian 0x55)
        if (peek_b == (INSTRUMENT_PROTO_MAGIC & 0xFF) && Serial.available() >= (int)sizeof(InstrumentPacketHeader)) {
            instrument_engine_process_stream(Serial, start_requested, stop_requested);
            return;
        }

        // Protocol Magic: Binary packets from Python Companion App (0x5A)
        if (peek_b == PROTOCOL_MAGIC && Serial.available() >= (int)sizeof(SerialHeader)) {
            serial_protocol_process(current_config, start_requested, stop_requested);
            return;
        }

        // Debug Subsystem Magic: Binary packets from DebugProtocol (0x5B)
        if (peek_b == 0x5B) {
            debug_engine::poll_stream(Serial);
            return;
        }

        // Interactive Human Terminal Engine (Line Editor, VT100, History)
        terminal_task();
        break;
    }
}

void setup() {
    serial_protocol_init();
    instrument_engine_init();
    interchip_init(INTERMCU_UART_BAUDRATE); // 1 Mbps Native Factory UART Transport (RP2040 GP0/GP1 <-> ESP32 IO22/IO19)

    frontend_init();

    acquisition_init();
    hardware_shells_init();
    bp_core_init();
    macro_init();
    pio_edge_analyzer_init();
    sump_init();
    ui_init();
    debug_engine::init();

    // Default Inter-MCU Transport is Native Factory UART (GP0/GP1).
    // Optional jumper-wired SPI transport (GP2..GP5) remains inactive at boot.

    // Initialize Universal Terminal Engine (Safe HiZ boot state & VT100 prompt)
    terminal_init();
}

void loop() {
    bool start_requested = false;
    bool stop_requested  = false;

    // 0. Process Hardware Trigger & Circular DMA Task
    if (pio_capture_is_running()) {
        pio_capture_task();
    }
    acquisition_poll();

    // 1. Process USB CDC CLI Commands & SUMP logic sniffer packets
    process_usb_cli(start_requested, stop_requested);

    if (sump_is_active()) {
        sump_task(Serial);
    }

    // 2. Process SPI Slave Mailbox (if active) and incoming Native Factory UART commands
    if (spi_slave_transport_is_active()) {
        spi_slave_transport_task(start_requested, stop_requested);
    }
    process_esp32_commands(start_requested, stop_requested);

    // 3. Process five-button navigation and UI state machine
    ui_task(start_requested, stop_requested);

    // 4. Start capture request via Unified Instrument API
    if (start_requested && !acquisition_is_running()) {
        current_config.trigger = trigger_engine_get_config();

        CaptureConfig acq_cfg;
        acq_cfg.sample_rate_hz   = current_config.sample_rate_hz;
        acq_cfg.sample_count     = current_config.sample_count;
        acq_cfg.enabled_channels = current_config.enabled_channels;
        acq_cfg.trigger          = current_config.trigger;

        acquisition_configure(acq_cfg);
        if (acquisition_arm()) {
            bp_set_mode(BP_MODE_LOGIC, &Serial);
            ui_notify_capture_complete();
        }
    }

    // 5. Stop capture request
    if (stop_requested && acquisition_is_running()) {
        acquisition_abort();
        ui_notify_capture_complete();
    }

    // 6. Handle capture completion via Unified Instrument API
    if (acquisition_is_ready()) {
        const CaptureResult& res = acquisition_get_result();
        acquisition_clear_ready();

        if (res.valid && res.samples) {
            ui_notify_capture_complete();

            // Stream binary capture buffer to Python Desktop App over USB CDC
            serial_protocol_send_buffer(res.samples, res.sample_count, res.sample_rate_hz);

            // Simultaneously stream binary capture buffer to ESP32 over 1Mbps UART fallback
            stream_buffer_to_esp32(res.samples, res.sample_count);
        }
    }

    // 7. Flush queued instrument events (markers, triggers, telemetry)
    instrument_engine_poll_events(Serial);
    instrument_engine_poll_events(ESP32_SERIAL);
}
