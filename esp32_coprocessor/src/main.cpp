#include <WiFi.h>
#include <esp_wifi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <NimBLEDevice.h>
#include <esp_timer.h>
#include "hardware_config.h"
#include "instrument_protocol.h"
#include "udoo_protocol.h"
#include "spi_master_transport.h"
#include "wireless_coprocessor_service.h"
#include "core/resource_manager.h"
#include "core/mode_manager.h"
#include "core/diagnostics.h"
#include "bus/gpio_service.h"
#include "bus/uart_service.h"
#include "bus/spi_service.h"
#include "bus/i2c_service.h"
#include "wireless/wifi_service.h"
#include "wireless/ble_service.h"
#include "rf/nrf24_service.h"
#include "ir/ir_service.h"
#include "packet/pcap_service.h"
#include "logic_scope_ui.h"

// Hardware Serial on ESP32 connected to RP2040 UART (Serial2: IO22 RX, IO19 TX)
#define RP2040_SERIAL Serial2

// Bit Pirate Pins on ESP32 (Non-conflicting pins)
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 26
#define PWM_OUT_PIN 25

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#pragma pack(push, 1)
struct WebLogicCaptureHeader {
    uint32_t magic;             // 0x53434F50 ('SCOP')
    uint16_t version;           // 0x0001
    uint8_t  channel_count;     // 8
    uint8_t  flags;             // bit 0: valid, bit 1: trigger_fired, bit 2: ext_trig
    uint32_t sample_rate_hz;    // sample rate in Hz
    uint32_t total_samples;     // total samples captured
    uint32_t trigger_offset;    // trigger sample offset
    uint32_t pre_samples;       // pre-trigger sample count
    uint32_t post_samples;      // post-trigger sample count
    uint32_t capture_id;        // monotonic capture id
};
#pragma pack(pop)

static const size_t MAX_SAMPLE_BUFFER_SIZE = 65536;
static uint8_t* last_sample_buffer = nullptr;
static uint32_t last_buffer_len = 0;
static WebLogicCaptureHeader last_capture_hdr = {
    0x53434F50, 1, 8, 1, 10000000, 8192, 0, 0, 8192, 0
};
static InstrumentStatusPayload last_rp2040_status = {};
static bool s_rp2040_online = false;
static uint32_t s_last_rp2040_ping_ms = 0;
static uint16_t s_esp32_tx_seq = 0;
static uint32_t s_last_wifi_scan_ms = 0;

static void send_instrument_packet(InstrumentPacketType type, const void* payload, uint16_t len) {
    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = (uint8_t)type;
    hdr.sequence    = ++s_esp32_tx_seq;
    hdr.payload_len = len;
    hdr.crc16       = (payload && len > 0) ? instrument_crc16((const uint8_t*)payload, len) : 0;

    RP2040_SERIAL.write((const uint8_t*)&hdr, sizeof(hdr));
    if (payload && len > 0) {
        RP2040_SERIAL.write((const uint8_t*)payload, len);
    }
    RP2040_SERIAL.flush();
}

// ─── ESP32 Bit Pirate Engine Functions ───────────────────────────────────────
String perform_i2c_scan() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    JsonDocument doc;
    JsonArray devices = doc["devices"].to<JsonArray>();

    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            char hex_str[10];
            snprintf(hex_str, sizeof(hex_str), "0x%02X", address);
            devices.add(hex_str);
        }
    }
    String out;
    serializeJson(doc, out);
    return out;
}

void configure_pwm_generator(uint32_t freq_hz, uint8_t duty_percent) {
    ledcSetup(0, freq_hz, 8); // Channel 0, 8-bit resolution (0-255)
    ledcAttachPin(PWM_OUT_PIN, 0);
    uint32_t duty_raw = (duty_percent * 255) / 100;
    ledcWrite(0, duty_raw);
}

// ─── Infrared (IR) Engine on ESP32 IO25 (TX) / IO26 (RX) ───────────────────
static uint8_t  s_ir_tx_pin = 25;
static uint8_t  s_ir_rx_pin = 26;
static uint16_t s_ir_carrier_freq_hz = 38000;
static uint8_t  s_ir_duty_percent = 33;
static bool     s_ir_rx_enabled = false;

void ir_configure(uint8_t tx_pin, uint8_t rx_pin, uint16_t freq_hz, uint8_t duty) {
    s_ir_tx_pin = tx_pin;
    s_ir_rx_pin = rx_pin;
    s_ir_carrier_freq_hz = freq_hz ? freq_hz : 38000;
    s_ir_duty_percent = duty ? duty : 33;
    pinMode(s_ir_rx_pin, INPUT_PULLUP);
}

void ir_send_raw(const uint16_t* timings_us, uint16_t count, uint16_t freq_hz = 38000) {
    ledcSetup(1, freq_hz, 8); // Channel 1 for IR carrier
    ledcAttachPin(s_ir_tx_pin, 1);
    uint32_t duty_val = (s_ir_duty_percent * 255) / 100;

    for (uint16_t i = 0; i < count; i++) {
        if ((i % 2) == 0) { // Mark: Carrier ON
            ledcWrite(1, duty_val);
        } else { // Space: Carrier OFF
            ledcWrite(1, 0);
        }
        delayMicroseconds(timings_us[i]);
    }
    ledcWrite(1, 0);
    ledcDetachPin(s_ir_tx_pin);
    pinMode(s_ir_tx_pin, INPUT);
}

void ir_send_nec(uint16_t addr, uint32_t cmd) {
    uint16_t timings[68];
    uint16_t idx = 0;
    timings[idx++] = 9000; // Leader mark
    timings[idx++] = 4500; // Leader space
    for (int b = 0; b < 16; b++) {
        timings[idx++] = 560;
        timings[idx++] = (addr & (1 << b)) ? 1690 : 560;
    }
    for (int b = 0; b < 8; b++) {
        timings[idx++] = 560;
        timings[idx++] = (cmd & (1 << b)) ? 1690 : 560;
    }
    for (int b = 0; b < 8; b++) {
        timings[idx++] = 560;
        timings[idx++] = ((~cmd) & (1 << b)) ? 1690 : 560;
    }
    timings[idx++] = 560; // Stop burst
    ir_send_raw(timings, idx, s_ir_carrier_freq_hz);
}

// ─── nRF24L01 Radio Engine on ESP32 UEXT Connector P3 ───────────────────────
static uint8_t s_nrf24_ce_pin   = 4;
static uint8_t s_nrf24_csn_pin  = 15;
static uint8_t s_nrf24_sck_pin  = 14;
static uint8_t s_nrf24_miso_pin = 35;
static uint8_t s_nrf24_mosi_pin = 12;
static uint8_t s_nrf24_channel  = 76;
static bool    s_nrf24_connected= false;

static uint8_t nrf24_spi_xfer(uint8_t byte_out) {
    uint8_t byte_in = 0;
    for (int i = 7; i >= 0; i--) {
        digitalWrite(s_nrf24_mosi_pin, (byte_out >> i) & 1);
        digitalWrite(s_nrf24_sck_pin, HIGH);
        delayMicroseconds(1);
        byte_in = (byte_in << 1) | (digitalRead(s_nrf24_miso_pin) & 1);
        digitalWrite(s_nrf24_sck_pin, LOW);
        delayMicroseconds(1);
    }
    return byte_in;
}

static uint8_t nrf24_read_reg(uint8_t reg) {
    digitalWrite(s_nrf24_csn_pin, LOW);
    nrf24_spi_xfer(reg & 0x1F);
    uint8_t val = nrf24_spi_xfer(0xFF);
    digitalWrite(s_nrf24_csn_pin, HIGH);
    return val;
}

static void nrf24_write_reg(uint8_t reg, uint8_t val) {
    digitalWrite(s_nrf24_csn_pin, LOW);
    nrf24_spi_xfer((reg & 0x1F) | 0x20);
    nrf24_spi_xfer(val);
    digitalWrite(s_nrf24_csn_pin, HIGH);
}

void nrf24_init() {
    pinMode(s_nrf24_ce_pin, OUTPUT);
    pinMode(s_nrf24_csn_pin, OUTPUT);
    pinMode(s_nrf24_sck_pin, OUTPUT);
    pinMode(s_nrf24_mosi_pin, OUTPUT);
    pinMode(s_nrf24_miso_pin, INPUT);

    digitalWrite(s_nrf24_ce_pin, LOW);
    digitalWrite(s_nrf24_csn_pin, HIGH);
    digitalWrite(s_nrf24_sck_pin, LOW);

    nrf24_write_reg(0x05, 0x4C); // Test write to RF_CH (ch 76)
    s_nrf24_connected = (nrf24_read_reg(0x05) == 0x4C);
}

// Note: index_html Web Application UI is defined in logic_scope_ui.h


void send_term_output(const String& msg, Print* serial_out, AsyncWebSocketClient* client) {
    if (serial_out) {
        serial_out->print(msg);
    }
    JsonDocument doc;
    doc["type"] = "term";
    doc["text"] = msg;
    String json;
    serializeJson(doc, json);
    if (client) {
        client->text(json);
    } else {
        ws.textAll(json);
    }
}

class TerminalOutputBridge : public Print {
public:
    TerminalOutputBridge(Print* serialOut, AsyncWebSocketClient* client)
        : m_serialOut(serialOut), m_client(client) {}

    size_t write(uint8_t c) override {
        if (m_serialOut) m_serialOut->write(c);
        m_line += (char)c;
        if (c == '\n' || m_line.length() >= 256) {
            flushLine();
        }
        return 1;
    }

    size_t write(const uint8_t* buffer, size_t size) override {
        for (size_t i = 0; i < size; i++) {
            write(buffer[i]);
        }
        return size;
    }

    void flushLine() {
        if (m_line.length() > 0) {
            JsonDocument doc;
            doc["type"] = "term";
            doc["text"] = m_line;
            String json;
            serializeJson(doc, json);
            if (m_client) {
                m_client->text(json);
            } else {
                ws.textAll(json);
            }
            m_line = "";
        }
    }

private:
    Print* m_serialOut;
    AsyncWebSocketClient* m_client;
    String m_line;
};

void run_mcu_spi_test(Print* serial_out, AsyncWebSocketClient* client) {
    send_term_output("\r\n==================================================\r\n", serial_out, client);
    send_term_output(" MCU SPI TRANSPORT SELF TEST (Section 15)\r\n", serial_out, client);
    send_term_output("==================================================\r\n", serial_out, client);

    send_term_output("1. Commanding RP2040 into INTERCHIP_SPI mode over Factory UART...\r\n", serial_out, client);
    const char* spi_cmd = "transport spi";
    PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_PIRATE_TERMINAL, (uint16_t)strlen(spi_cmd) };
    RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
    RP2040_SERIAL.write((const uint8_t*)spi_cmd, strlen(spi_cmd));
    delay(50);

    const char* synth_cmd = "test synth 8192";
    PacketHeader hdr2 = { PROTOCOL_SYNC_BYTE, PKT_PIRATE_TERMINAL, (uint16_t)strlen(synth_cmd) };
    RP2040_SERIAL.write((uint8_t*)&hdr2, sizeof(hdr2));
    RP2040_SERIAL.write((const uint8_t*)synth_cmd, strlen(synth_cmd));
    delay(50);

    send_term_output("2. Initializing ESP32 UEXT SPI Master (10 MHz)...\r\n", serial_out, client);
    spi_master_transport_init();

    send_term_output("3. Executing 1000-packet verification test suite...\r\n", serial_out, client);
    McuSpiTestReport report = {};
    bool ok = spi_master_run_mcu_spi_test(report);

    String rep = "\r\nMCU SPI SELF TEST\r\n-----------------\r\n";
    rep += "Clock                 : 10 MHz\r\n";
    rep += "TX packets            : " + String(report.tx_packets) + "\r\n";
    rep += "RX packets            : " + String(report.rx_packets) + "\r\n";
    rep += "CRC failures          : " + String(report.crc_failures) + "\r\n";
    rep += "Sequence errors       : " + String(report.sequence_errors) + "\r\n";
    rep += "Timeouts              : " + String(report.timeouts) + "\r\n";
    rep += "Data mismatches       : " + String(report.data_mismatches) + "\r\n\r\n";
    rep += "RESULT: " + String(ok ? "PASS" : "FAIL (Check jumper wires: IO14/12/35/15 <-> GP2..GP5)") + "\r\n";
    send_term_output(rep, serial_out, client);

    send_term_output("4. Restoring Factory UART and releasing SPI...\r\n", serial_out, client);
    spi_master_transport_stop();

    const char* uart_cmd = "transport uart";
    PacketHeader hdr3 = { PROTOCOL_SYNC_BYTE, PKT_PIRATE_TERMINAL, (uint16_t)strlen(uart_cmd) };
    RP2040_SERIAL.write((uint8_t*)&hdr3, sizeof(hdr3));
    RP2040_SERIAL.write((const uint8_t*)uart_cmd, strlen(uart_cmd));
    send_term_output("> ", serial_out, client);
}

void run_mcu_reset_test(Print* serial_out, AsyncWebSocketClient* client) {
    send_term_output("\r\n=== RP2040 HARDWARE RESET & RECOVERY TEST ===\r\n", serial_out, client);
    send_term_output("1. Checking RP2040 baseline responsiveness over UART...\r\n", serial_out, client);

    while (RP2040_SERIAL.available()) RP2040_SERIAL.read();

    const char* ping_cmd = "ping";
    PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_PIRATE_TERMINAL, (uint16_t)strlen(ping_cmd) };
    RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
    RP2040_SERIAL.write((const uint8_t*)ping_cmd, strlen(ping_cmd));

    unsigned long t0 = millis();
    bool responsive = false;
    while (millis() - t0 < 300) {
        if (RP2040_SERIAL.available()) {
            responsive = true;
            while (RP2040_SERIAL.available()) RP2040_SERIAL.read();
            break;
        }
        delay(5);
    }
    send_term_output(String("   Baseline Status: ") + (responsive ? "ONLINE / RESPONSIVE" : "NO RESPONSE (Continuing)") + "\r\n", serial_out, client);

    send_term_output("2. Asserting ESP32 IO23 LOW (Holding RP2040 in hardware reset)...\r\n", serial_out, client);
    rp2040_reset_assert();
    delay(50);

    send_term_output("3. Releasing ESP32 IO23 HIGH (RP2040 booting)...\r\n", serial_out, client);
    unsigned long t_release = millis();
    rp2040_reset_release();

    send_term_output("4. Waiting for RP2040 bootloader & firmware startup...\r\n", serial_out, client);
    bool recovered = false;
    unsigned long recovery_latency_ms = 0;
    while (millis() - t_release < 2000) {
        if (RP2040_SERIAL.available()) {
            recovered = true;
            recovery_latency_ms = millis() - t_release;
            while (RP2040_SERIAL.available()) RP2040_SERIAL.read();
            break;
        }
        if ((millis() - t_release) % 100 < 10) {
            RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
            RP2040_SERIAL.write((const uint8_t*)ping_cmd, strlen(ping_cmd));
        }
        delay(10);
    }

    send_term_output("\r\n--- RESET & RECOVERY TEST REPORT ---\r\n", serial_out, client);
    send_term_output("Reset Line (IO23)    : TESTED (Asserted LOW, Released HIGH)\r\n", serial_out, client);
    send_term_output(String("Recovery Status      : ") + (recovered ? "RECOVERED / VERIFIED OK" : "FAILED / TIMEOUT") + "\r\n", serial_out, client);
    if (recovered) {
        send_term_output("Recovery Latency     : " + String(recovery_latency_ms) + " ms\r\n", serial_out, client);
        send_term_output("\r\nRESULT: PASS\r\n> ", serial_out, client);
    } else {
        send_term_output("\r\nRESULT: FAIL\r\n> ", serial_out, client);
    }
}

// Section 17: Onboard RP2040 ARM CoreSight SWD Self-Interrogation (ESP32 IO2/IO4/IO5/IO23)
uint32_t run_onboard_swd_probe(Print* serial_out, AsyncWebSocketClient* client) {
    send_term_output("\r\n[SWD] Interrogating Onboard RP2040 Target via Hardwired SWD link (IO2/4/5)...\r\n", serial_out, client);

    // Enable SWD buffer: GPIO5 HIGH
    pinMode(ESP32_PIN_SWD_OE, OUTPUT);
    digitalWrite(ESP32_PIN_SWD_OE, HIGH);

    pinMode(ESP32_PIN_SWCLK, OUTPUT);
    digitalWrite(ESP32_PIN_SWCLK, LOW);
    pinMode(ESP32_PIN_SWDIO, OUTPUT);
    digitalWrite(ESP32_PIN_SWDIO, HIGH);

    auto swd_delay = []() { delayMicroseconds(1); };
    auto swd_pulse = [&]() {
        digitalWrite(ESP32_PIN_SWCLK, LOW); swd_delay();
        digitalWrite(ESP32_PIN_SWCLK, HIGH); swd_delay();
    };

    // 1. Line reset (55 clocks with SWDIO HIGH)
    for (int i = 0; i < 55; ++i) swd_pulse();

    // 2. JTAG-to-SWD sequence: 0xE79E (LSB first)
    uint16_t seq = 0xE79E;
    for (int i = 0; i < 16; ++i) {
        digitalWrite(ESP32_PIN_SWCLK, LOW);
        digitalWrite(ESP32_PIN_SWDIO, (seq >> i) & 1);
        swd_delay();
        digitalWrite(ESP32_PIN_SWCLK, HIGH);
        swd_delay();
    }

    // 3. Line reset again
    digitalWrite(ESP32_PIN_SWDIO, HIGH);
    for (int i = 0; i < 55; ++i) swd_pulse();

    // 4. Idle cycles
    digitalWrite(ESP32_PIN_SWDIO, LOW);
    for (int i = 0; i < 16; ++i) swd_pulse();

    // 5. Read DPIDR (A[3:2] = 0b00, AP=0, RnW=1) -> Header = 0xA5
    uint8_t hdr = 0xA5;
    for (int i = 0; i < 8; ++i) {
        digitalWrite(ESP32_PIN_SWCLK, LOW);
        digitalWrite(ESP32_PIN_SWDIO, (hdr >> i) & 1);
        swd_delay();
        digitalWrite(ESP32_PIN_SWCLK, HIGH);
        swd_delay();
    }

    // Turnaround
    pinMode(ESP32_PIN_SWDIO, INPUT_PULLUP);
    swd_pulse();

    // Read ACK (3 bits)
    uint8_t ack = 0;
    for (int i = 0; i < 3; ++i) {
        digitalWrite(ESP32_PIN_SWCLK, LOW); swd_delay();
        if (digitalRead(ESP32_PIN_SWDIO)) ack |= (1 << i);
        digitalWrite(ESP32_PIN_SWCLK, HIGH); swd_delay();
    }

    uint32_t dpidr = 0;
    if (ack == 1) { // OK
        for (int i = 0; i < 32; ++i) {
            digitalWrite(ESP32_PIN_SWCLK, LOW); swd_delay();
            if (digitalRead(ESP32_PIN_SWDIO)) dpidr |= (1u << i);
            digitalWrite(ESP32_PIN_SWCLK, HIGH); swd_delay();
        }
        // Parity
        swd_pulse();
    }

    // Safely release pins and disable buffer
    pinMode(ESP32_PIN_SWDIO, INPUT);
    pinMode(ESP32_PIN_SWCLK, INPUT);
    digitalWrite(ESP32_PIN_SWD_OE, LOW);
    pinMode(ESP32_PIN_SWD_OE, INPUT);

    String rep;
    if (ack == 1 && dpidr != 0 && dpidr != 0xFFFFFFFF) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[SWD PASS] RP2040 Target Connected!\r\n  DPIDR = 0x%08X (ARM Cortex-M0+ CoreSight SW-DP)\r\n> ", (unsigned int)dpidr);
        rep = String(buf);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "[SWD FAIL] Target ACK=%d (Expected 1/OK), DPIDR=0x%08X\r\n> ", ack, (unsigned int)dpidr);
        rep = String(buf);
    }

    send_term_output(rep, serial_out, client);
    return dpidr;
}

void handle_terminal_cmd(const char* txt, Print* serial_out, AsyncWebSocketClient* client) {
    if (!txt || !*txt) return;
    while (isspace(*txt)) txt++;
    if (!*txt) return;

    TerminalOutputBridge bridge(serial_out, client);

    // 1. Bus Pirate Mode Switching: 'm <mode>' or 'mode <mode>'
    if (strncmp(txt, "m ", 2) == 0 || strncmp(txt, "mode ", 5) == 0 || strcmp(txt, "m") == 0 || strcmp(txt, "mode") == 0) {
        const char* target = (strncmp(txt, "m ", 2) == 0) ? (txt + 2) : (strncmp(txt, "mode ", 5) == 0 ? (txt + 5) : "");
        while (isspace(*target)) target++;
        if (!*target) {
            String list = "\r\nAvailable modes:\r\n"
                          "  0. HiZ   - High impedance / safe idle state\r\n"
                          "  1. GPIO  - Digital pin read, write, mode control\r\n"
                          "  2. UART  - User UART monitor & transmission\r\n"
                          "  3. SPI   - Hardware SPI master read, write, xfer\r\n"
                          "  4. I2C   - Hardware I2C bus scan, read, write\r\n"
                          "  5. WIFI  - Wi-Fi recon, connection, ping, sniffer\r\n"
                          "  6. BLE   - Bluetooth LE scanner & device inspector\r\n"
                          "  7. SNIFF - PCAP packet sniffer buffer & export\r\n"
                          "  8. IR    - Infrared transmit & receive/decode\r\n"
                          "  9. NRF24 - nRF24L01+ 2.4GHz RF transceiver\r\n";
            send_term_output(list, serial_out, client);
            return;
        }
        Esp32Core::CoprocessorMode mode = Esp32Core::parseMode(target);
        if (mode == Esp32Core::CoprocessorMode::COUNT) {
            send_term_output(String("\r\nUnknown mode: '") + target + "'. Type 'm' to list modes.\r\n", serial_out, client);
        } else {
            Esp32Core::ModeManager::instance().setMode(mode, &bridge);
            bridge.flushLine();
        }
        return;
    }

    // 2. Global Help
    if (strcmp(txt, "help") == 0 || strcmp(txt, "?") == 0) {
        Esp32Core::CoprocessorMode curMode = Esp32Core::ModeManager::instance().currentMode();
        String h = "\r\n=== UDOO KEY ESP32 COPROCESSOR (Phase 2) ===\r\n";
        h += "Active Mode   : " + String(Esp32Core::modeToString(curMode)) + "\r\n";
        h += "Mode Switch   : m <mode_name or 0-9> (e.g. 'm wifi', 'm spi', 'm hiz')\r\n\r\n";
        h += "Global Commands:\r\n";
        h += "  m / mode      : List or switch modes\r\n";
        h += "  info / sysinfo: Show system resources, memory, and coprocessor status\r\n";
        h += "  diag / selftest: Run comprehensive coprocessor self-test matrix\r\n";
        h += "  status        : Print status of current mode subsystem\r\n";
        h += "  pcap <cmd>    : PCAP packet capture buffer control (status/hex/bin/clear)\r\n";
        h += "  transport     : Inter-MCU transport control (uart/spi/status)\r\n";
        h += "  swd           : Run onboard SWD target interrogation\r\n";
        h += "  rp2040 <cmd>  : Forward command directly to RP2040 logic analyzer\r\n";
        h += "\r\nDirect Mode Commands (any mode):\r\n";
        h += "  gpio <args> | uart <args> | spi <args> | i2c <args>\r\n";
        h += "  wifi <args> | ble <args> | ir <args> | nrf24 <args>\r\n";
        send_term_output(h, serial_out, client);
        return;
    }

    // 3. Global System Info & Diagnostics
    if (strcmp(txt, "info") == 0 || strcmp(txt, "sysinfo") == 0) {
        Esp32Core::Diagnostics::dumpSystemInfo(bridge);
        bridge.flushLine();
        return;
    }

    if (strcmp(txt, "diag") == 0 || strcmp(txt, "self") == 0 || strcmp(txt, "selftest") == 0) {
        Esp32Core::Diagnostics::runSelfTest(bridge, &RP2040_SERIAL);
        bridge.flushLine();
        return;
    }

    // 4. Global PCAP Packet Commands
    if (strncmp(txt, "pcap", 4) == 0) {
        const char* sub = txt + 4;
        while (isspace(*sub)) sub++;
        Esp32Packet::PcapService::instance().executeCommand(sub, bridge);
        bridge.flushLine();
        return;
    }

    // 5. Direct Subsystem Command Prefixes (Usable from ANY mode)
    if (strncmp(txt, "gpio", 4) == 0) {
        const char* sub = txt + 4;
        while (isspace(*sub)) sub++;
        Esp32Bus::GpioService::instance().executeCommand(sub, bridge);
        bridge.flushLine();
        return;
    }

    if (strncmp(txt, "uart", 4) == 0) {
        const char* sub = txt + 4;
        while (isspace(*sub)) sub++;
        Esp32Bus::UartService::instance().executeCommand(sub, bridge);
        bridge.flushLine();
        return;
    }

    if (strncmp(txt, "spi", 3) == 0 && strncmp(txt, "spi_flash", 9) != 0) {
        const char* sub = txt + 3;
        while (isspace(*sub)) sub++;
        Esp32Bus::SpiService::instance().executeCommand(sub, bridge);
        bridge.flushLine();
        return;
    }

    if (strncmp(txt, "i2c", 3) == 0) {
        const char* sub = txt + 3;
        while (isspace(*sub)) sub++;
        Esp32Bus::I2cService::instance().executeCommand(sub, bridge);
        bridge.flushLine();
        return;
    }

    if (strncmp(txt, "wifi", 4) == 0) {
        const char* sub = txt + 4;
        while (isspace(*sub)) sub++;
        Esp32Wireless::WifiService::instance().executeCommand(sub, bridge);
        bridge.flushLine();
        return;
    }

    if (strncmp(txt, "ble", 3) == 0 || strncmp(txt, "bt", 2) == 0) {
        const char* sub = (strncmp(txt, "ble", 3) == 0) ? (txt + 3) : (txt + 2);
        while (isspace(*sub)) sub++;
        Esp32Wireless::BleService::instance().executeCommand(sub, bridge);
        bridge.flushLine();
        return;
    }

    if (strncmp(txt, "ir", 2) == 0) {
        const char* sub = txt + 2;
        while (isspace(*sub)) sub++;
        Esp32Ir::IrService::instance().executeCommand(sub, bridge);
        bridge.flushLine();
        return;
    }

    if (strncmp(txt, "nrf24", 5) == 0 || strncmp(txt, "rf24", 4) == 0) {
        const char* sub = (strncmp(txt, "nrf24", 5) == 0) ? (txt + 5) : (txt + 4);
        while (isspace(*sub)) sub++;
        Esp32Rf::Nrf24Service::instance().executeCommand(sub, bridge);
        bridge.flushLine();
        return;
    }

    // 6. Explicit RP2040 forward command
    if (strncmp(txt, "rp2040 ", 7) == 0 || strncmp(txt, "logic ", 6) == 0) {
        const char* sub = (strncmp(txt, "rp2040 ", 7) == 0) ? (txt + 7) : (txt + 6);
        while (isspace(*sub)) sub++;
        uint16_t txt_len = strlen(sub);
        PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_PIRATE_TERMINAL, txt_len };
        RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
        RP2040_SERIAL.write((const uint8_t*)sub, txt_len);
        return;
    }

    // 7. Mode-Contextual Dispatch (Commands interpreted according to current mode)
    Esp32Core::CoprocessorMode currentMode = Esp32Core::ModeManager::instance().currentMode();
    switch (currentMode) {
        case Esp32Core::CoprocessorMode::GPIO:
            Esp32Bus::GpioService::instance().executeCommand(txt, bridge);
            bridge.flushLine();
            return;
        case Esp32Core::CoprocessorMode::UART:
            Esp32Bus::UartService::instance().executeCommand(txt, bridge);
            bridge.flushLine();
            return;
        case Esp32Core::CoprocessorMode::SPI:
            Esp32Bus::SpiService::instance().executeCommand(txt, bridge);
            bridge.flushLine();
            return;
        case Esp32Core::CoprocessorMode::I2C:
            Esp32Bus::I2cService::instance().executeCommand(txt, bridge);
            bridge.flushLine();
            return;
        case Esp32Core::CoprocessorMode::WIFI:
            Esp32Wireless::WifiService::instance().executeCommand(txt, bridge);
            bridge.flushLine();
            return;
        case Esp32Core::CoprocessorMode::BLE:
            Esp32Wireless::BleService::instance().executeCommand(txt, bridge);
            bridge.flushLine();
            return;
        case Esp32Core::CoprocessorMode::SNIFF:
            Esp32Packet::PcapService::instance().executeCommand(txt, bridge);
            bridge.flushLine();
            return;
        case Esp32Core::CoprocessorMode::IR:
            Esp32Ir::IrService::instance().executeCommand(txt, bridge);
            bridge.flushLine();
            return;
        case Esp32Core::CoprocessorMode::NRF24:
            Esp32Rf::Nrf24Service::instance().executeCommand(txt, bridge);
            bridge.flushLine();
            return;
        case Esp32Core::CoprocessorMode::HIZ:
        default:
            break;
    }

    // 8. Inter-MCU Transport Commands (transport spi / transport uart / transport status)
    if (strncmp(txt, "transport", 9) == 0) {
        const char* sub = txt + 9;
        while (isspace(*sub)) sub++;
        if (strncmp(sub, "spi", 3) == 0) {
            send_term_output("\r\nEnabling jumper-wired MCU SPI...\r\n\r\n"
                             "  ESP32 IO14 -> RP2040 GP2 : SCK\r\n"
                             "  ESP32 IO12 -> RP2040 GP3 : MOSI\r\n"
                             "  ESP32 IO35 -> RP2040 GP4 : MISO\r\n"
                             "  ESP32 IO15 -> RP2040 GP5 : CS\r\n"
                             "  GND        -> GND\r\n\r\n"
                             "WARNING:\r\n"
                             "  GP2-GP5 are now reserved for inter-chip transport.\r\n"
                             "  Logic analyzer and external SPI probe modes are unavailable.\r\n\r\n", serial_out, client);
            const char* cmd = "transport spi";
            PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_PIRATE_TERMINAL, (uint16_t)strlen(cmd) };
            RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
            RP2040_SERIAL.write((const uint8_t*)cmd, strlen(cmd));
            delay(20);
            spi_master_transport_init();
            send_term_output("MCU SPI transport: ACTIVE\r\nClock: 10 MHz\r\n> ", serial_out, client);
            return;
        } else if (strncmp(sub, "uart", 4) == 0) {
            send_term_output("\r\nRestoring Native Factory UART transport (IO22 RX / IO19 TX)...\r\n", serial_out, client);
            spi_master_transport_stop();
            const char* cmd = "transport uart";
            PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_PIRATE_TERMINAL, (uint16_t)strlen(cmd) };
            RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
            RP2040_SERIAL.write((const uint8_t*)cmd, strlen(cmd));
            send_term_output("MCU UART transport: ACTIVE\r\n> ", serial_out, client);
            return;
        } else {
            bool spi_act = spi_master_transport_is_active();
            String rep = "\r\n==================================================\r\n";
            rep += " MCU TRANSPORT\r\n";
            rep += "==================================================\r\n";
            rep += " Factory UART       : IO22/IO19 (Hardwired to RP2040 GP0/GP1)\r\n";
            rep += " UART status        : AVAILABLE (Default Inter-MCU Bus)\r\n\r\n";
            rep += " Jumper SPI         : UEXT IO14/IO12/IO35/IO15 (Optional Jumpers to GP2..GP5)\r\n";
            rep += " SPI clock          : 10 MHz\r\n";
            rep += " SPI status         : " + String(spi_act ? "ACTIVE" : "NOT CONFIGURED / IDLE") + "\r\n\r\n";
            rep += " Current transport  : " + String(spi_act ? "INTERCHIP_SPI (Optional Jumper)" : "UART (Native Factory)") + "\r\n";
            rep += "==================================================\r\n> ";
            send_term_output(rep, serial_out, client);
            return;
        }
    }

    if (strncmp(txt, "swd", 3) == 0) {
        run_onboard_swd_probe(serial_out, client);
        return;
    }

    if (strncmp(txt, "sync", 4) == 0) {
        bool ok = wireless_diag_sync_clock();
        String res = ok ? ("\r\n[ClockSync] RP2040 microsecond sync successful! Offset: " + String((long)wireless_diag_get_clock_offset_us()) + " us\r\n> ")
                        : "\r\n[ClockSync] RP2040 sync failed.\r\n> ";
        send_term_output(res, serial_out, client);
        return;
    }

    if (strncmp(txt, "probe", 5) == 0) {
        const char* host = txt + 5;
        while (*host == ' ') host++;
        if (*host == '\0') host = "one.one.one.one";
        String probe = wireless_diag_run_network_probe(host);
        send_term_output(probe + "\r\n> ", serial_out, client);
        return;
    }

    // 9. Production Verification & Hardware Tests
    if (strncmp(txt, "test", 4) == 0 || strncmp(txt, "bench", 5) == 0) {
        const char* sub = (strncmp(txt, "test", 4) == 0) ? (txt + 4) : txt;
        while (isspace(*sub)) sub++;

        if (strncmp(sub, "swd", 3) == 0) {
            run_onboard_swd_probe(serial_out, client);
            return;
        }
        if (strncmp(sub, "mcu_spi", 7) == 0) {
            run_mcu_spi_test(serial_out, client);
            return;
        }
        if (strncmp(sub, "mcu_reset", 9) == 0 || strncmp(sub, "reset", 5) == 0) {
            run_mcu_reset_test(serial_out, client);
            return;
        }
        if (strncmp(sub, "synth", 5) == 0 || strncmp(sub, "ramp", 4) == 0) {
            uint32_t samples = 8192;
            const char* num_str = strchr(sub, ' ');
            if (num_str) {
                uint32_t s = atoi(num_str);
                if (s > 0) samples = (s > MAX_SAMPLE_BUFFER_SIZE) ? MAX_SAMPLE_BUFFER_SIZE : s;
            }

            char cmd_req[32];
            snprintf(cmd_req, sizeof(cmd_req), "test synth %lu", (unsigned long)samples);
            char resp[64] = {0};
            spi_master_exec_command(cmd_req, resp, sizeof(resp));

            uint32_t t_wait = millis();
            while (!spi_master_is_data_ready() && (millis() - t_wait < 250)) {
                delayMicroseconds(100);
            }

            if (!spi_master_is_data_ready()) {
                send_term_output("\r\n[TEST 1 FAIL] Timeout awaiting RP2040 DATA_READY line assertion!\r\n> ", serial_out, client);
                return;
            }

            SpiCaptureInfoPayload info;
            uint32_t t_xfer_start = micros();
            bool ok = spi_master_fetch_complete_capture(last_sample_buffer, MAX_SAMPLE_BUFFER_SIZE, info);
            uint32_t t_xfer_us = micros() - t_xfer_start;

            if (ok) {
                uint32_t first_err = 0;
                bool pass = spi_master_test_verify_synthetic(last_sample_buffer, info.total_samples, first_err);
                last_buffer_len = info.total_samples;
                ws.binaryAll((char*)last_sample_buffer, last_buffer_len);

                float rate_mb_s = (t_xfer_us > 0) ? (((float)info.total_samples / (1024.0f * 1024.0f)) / ((float)t_xfer_us / 1000000.0f)) : 0.0f;

                if (pass) {
                    String msg = "\r\n[TEST 1 PASS] 00..FF Synthetic Ramp Verified across " + String(info.total_samples) + " samples (" +
                                 String(info.total_blocks) + " blocks)!\r\n" +
                                 "  Zero byte-shifts, zero DMA boundary drops, zero packet tears.\r\n" +
                                 "  Capture Generation : #" + String(info.capture_id) + "\r\n" +
                                 "  Wire Transfer Time : " + String(t_xfer_us / 1000.0f, 2) + " ms (" + String(rate_mb_s, 2) + " MB/s payload throughput)\r\n" +
                                 "  Whole Capture CRC  : 0x" + String(info.whole_capture_crc, HEX) + " (Level 2 Verified)\r\n" +
                                 "  Scope trace updated on web canvas.\r\n> ";
                    send_term_output(msg, serial_out, client);
                } else {
                    String msg = "\r\n[TEST 1 FAIL] Byte mismatch at sample offset " + String(first_err) + "!\r\n" +
                                 "  Expected: 0x" + String((uint8_t)(first_err & 0xFF), HEX) + ", Received: 0x" +
                                 String(last_sample_buffer[first_err], HEX) + "\r\n> ";
                    send_term_output(msg, serial_out, client);
                }
            } else {
                send_term_output("\r\n[TEST 1 FAIL] SPI transport fetch failed (CRC mismatch or invariant violation)!\r\n> ", serial_out, client);
            }
            return;
        }

        if (strncmp(sub, "bench", 5) == 0) {
            uint32_t iterations = 10;
            const char* num_str = strchr(sub, ' ');
            if (num_str) {
                uint32_t n = atoi(num_str);
                if (n > 0) iterations = (n > 50) ? 50 : n;
            }

            send_term_output("\r\n[BENCHMARK] Executing " + String(iterations) + " continuous captures over 10 MHz SPI...\r\n", serial_out, client);
            SpiBenchmarkReport rep;
            bool ok = spi_master_run_benchmark(iterations, rep);

            String report_str = "=== SPI 10MHz TRANSPORT BENCHMARK RESULTS ===\r\n";
            report_str += "  Transfers Completed : " + String(rep.total_transfers) + " / " + String(iterations) + "\r\n";
            report_str += "  Total Payload Bytes : " + String(rep.total_bytes) + " bytes (" + String(rep.total_bytes / 1024.0f, 1) + " KB)\r\n";
            report_str += "  Total Elapsed Time  : " + String(rep.total_time_ms) + " ms\r\n";
            report_str += "  Effective Throughput: " + String(rep.effective_mb_s, 2) + " MB/s (payload wire rate)\r\n";
            report_str += "  Block CRC Errors    : " + String(rep.crc_errors) + "\r\n";
            report_str += "  Block Retries       : " + String(rep.retries) + "\r\n";
            report_str += "  Generation Races    : " + String(rep.generation_mismatches) + "\r\n";
            report_str += "  Transport Status    : " + String(ok ? "100% RELIABLE (PASSED)" : "COMPLETED WITH ERRORS") + "\r\n> ";
            send_term_output(report_str, serial_out, client);
            return;
        }

        if (strncmp(sub, "ping", 4) == 0) {
            uint32_t t_ping = micros();
            bool pong = spi_master_ping();
            uint32_t dur = micros() - t_ping;
            if (pong) {
                send_term_output("\r\n[SPI Master] PONG received from RP2040 in " + String(dur) + " us (Magic 0x5350 verified)!\r\n> ", serial_out, client);
            } else {
                send_term_output("\r\n[SPI Master] Ping failed - no response from RP2040 slave.\r\n> ", serial_out, client);
            }
            return;
        }

        if (strncmp(sub, "status", 6) == 0) {
            SpiStatusPayload stat;
            bool ok = spi_master_get_status(stat);
            bool dr = spi_master_is_data_ready();
            uint32_t retries, mismatches, errs;
            spi_master_get_stats(retries, mismatches, errs);

            String status_str = "\r\n=== SPI TRANSPORT HARDWARE STATUS ===\r\n";
            status_str += "  DATA_READY Pin (GPIO 4) : " + String(dr ? "HIGH (Asserted)" : "LOW (Idle)") + "\r\n";
            status_str += "  RP2040 Slave Status     : " + String(ok ? "CONNECTED & RESPONDING" : "NO RESPONSE") + "\r\n";
            if (ok) {
                status_str += "  Current Capture ID      : #" + String(stat.capture_id) + "\r\n";
                status_str += "  Acquisition Running     : " + String(stat.capture_running ? "YES" : "NO") + "\r\n";
                status_str += "  Capture Ready in SRAM   : " + String(stat.capture_ready ? "YES" : "NO") + "\r\n";
                status_str += "  Configured Sample Rate  : " + String(stat.sample_rate_hz) + " Hz\r\n";
                status_str += "  Configured Sample Count : " + String(stat.sample_count) + " samples\r\n";
            }
            status_str += "  Cumulative Retries      : " + String(retries) + "\r\n";
            status_str += "  Cumulative Race Drops   : " + String(mismatches) + "\r\n";
            status_str += "  Cumulative CRC Errors   : " + String(errs) + "\r\n> ";
            send_term_output(status_str, serial_out, client);
            return;
        }

        // Forward other test commands to RP2040
        char resp[256] = {0};
        if (spi_master_transport_is_active() && spi_master_exec_command(txt, resp, sizeof(resp)) && strlen(resp) > 0) {
            send_term_output(String(resp) + "\r\n> ", serial_out, client);
        } else {
            uint16_t txt_len = strlen(txt);
            PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_PIRATE_TERMINAL, txt_len };
            RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
            RP2040_SERIAL.write((const uint8_t*)txt, txt_len);
        }
        return;
    }

    // 10. Default: Forward command to RP2040 via SPI mailbox if active, falling back to Native Factory UART
    char resp[256] = {0};
    if (spi_master_transport_is_active() && spi_master_exec_command(txt, resp, sizeof(resp)) && strlen(resp) > 0) {
        send_term_output(String(resp) + "\r\n> ", serial_out, client);
    } else {
        uint16_t txt_len = strlen(txt);
        PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_PIRATE_TERMINAL, txt_len };
        RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
        RP2040_SERIAL.write((const uint8_t*)txt, txt_len);
    }
}

void process_usb_serial() {
    static char s_buf[128];
    static uint8_t s_idx = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (s_idx > 0) {
                s_buf[s_idx] = '\0';
                Serial.println();
                handle_terminal_cmd(s_buf, &Serial, nullptr);
                s_idx = 0;
                Serial.print("\r\n");
                Serial.print(Esp32Core::ModeManager::instance().prompt());
            }
        } else if (c == '\b' || c == 127) {
            if (s_idx > 0) {
                s_idx--;
                Serial.print("\b \b");
            }
        } else if (s_idx < sizeof(s_buf) - 1) {
            s_buf[s_idx++] = c;
            Serial.write(c);
        }
    }
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        JsonDocument resp;
        resp["type"] = "status_resp";
        resp["state"] = (last_rp2040_status.acquisition_state == 1) ? "ARMED" :
                        (last_rp2040_status.acquisition_state == 2) ? "TRIGGERED" :
                        (last_rp2040_status.acquisition_state == 3) ? "READY" : "IDLE";
        resp["rate"] = last_capture_hdr.sample_rate_hz;
        resp["samples"] = last_capture_hdr.total_samples;
        resp["trigger_offset"] = last_capture_hdr.trigger_offset;
        resp["capture_id"] = last_capture_hdr.capture_id;
        resp["adc0"] = last_rp2040_status.adc0_mv;
        resp["adc1"] = last_rp2040_status.adc1_mv;
        resp["adc2"] = last_rp2040_status.adc2_mv;
        String resJson;
        serializeJson(resp, resJson);
        client->text(resJson);
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->opcode == WS_TEXT) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, (const char*)data, len);
            if (err) return;

            const char* cmd = doc["cmd"];
            if (!cmd) return;

            if (strcmp(cmd, "start") == 0 || strcmp(cmd, "arm") == 0 || strcmp(cmd, "single") == 0) {
                uint32_t rate  = doc["rate"] | 10000000;
                uint32_t count = doc["count"] | 8192;
                uint8_t  trigCh = doc["trigger_channel"] | 0;
                uint8_t  trigMode = doc["trigger_mode"] | 0;
                uint8_t  preTrig = doc["pre_trigger_pct"] | 70;

                last_capture_hdr.sample_rate_hz = rate;
                last_capture_hdr.total_samples = count;

                InstrumentConfigPayload cfg;
                memset(&cfg, 0, sizeof(cfg));
                cfg.sample_rate_hz   = rate;
                cfg.sample_count     = count;
                cfg.trigger_channel  = trigCh;
                cfg.trigger_mode     = trigMode;
                cfg.pattern_mask     = 0;
                cfg.pattern_value    = 0;
                cfg.trigger_param    = 0;
                cfg.pre_trigger_pct  = preTrig;

                send_instrument_packet(PKT_TYPE_SET_CONFIG, &cfg, sizeof(cfg));
                send_instrument_packet(PKT_TYPE_START_CAPTURE, nullptr, 0);

                if (spi_master_transport_is_active()) {
                    spi_master_start_capture(rate, count);
                }

                PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_LA_START, 0 };
                RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr)); // UART Fallback

                client->text("{\"type\":\"logic_status\",\"state\":\"ARMED\"}");
            } else if (strcmp(cmd, "stop") == 0) {
                send_instrument_packet(PKT_TYPE_STOP_CAPTURE, nullptr, 0);
                PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_LA_STOP, 0 };
                RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
                client->text("{\"type\":\"logic_status\",\"state\":\"STOPPED\"}");
            } else if (strcmp(cmd, "status") == 0) {
                send_instrument_packet(PKT_TYPE_GET_STATUS, nullptr, 0);
                JsonDocument resp;
                resp["type"] = "status_resp";
                resp["state"] = (last_rp2040_status.acquisition_state == 1) ? "ARMED" :
                                (last_rp2040_status.acquisition_state == 2) ? "TRIGGERED" :
                                (last_rp2040_status.acquisition_state == 3) ? "READY" : "IDLE";
                resp["rate"] = last_capture_hdr.sample_rate_hz;
                resp["samples"] = last_capture_hdr.total_samples;
                resp["trigger_offset"] = last_capture_hdr.trigger_offset;
                resp["capture_id"] = last_capture_hdr.capture_id;
                resp["adc0"] = last_rp2040_status.adc0_mv;
                resp["adc1"] = last_rp2040_status.adc1_mv;
                resp["adc2"] = last_rp2040_status.adc2_mv;
                String resJson;
                serializeJson(resp, resJson);
                client->text(resJson);
            } else if (strcmp(cmd, "get_capture") == 0) {
                if (last_buffer_len > 0) {
                    size_t total_payload = sizeof(WebLogicCaptureHeader) + last_buffer_len;
                    uint8_t* tx_buf = (uint8_t*)malloc(total_payload);
                    if (tx_buf) {
                        memcpy(tx_buf, &last_capture_hdr, sizeof(WebLogicCaptureHeader));
                        memcpy(tx_buf + sizeof(WebLogicCaptureHeader), last_sample_buffer, last_buffer_len);
                        client->binary((char*)tx_buf, total_payload);
                        free(tx_buf);
                    } else {
                        client->binary((char*)last_sample_buffer, last_buffer_len);
                    }
                }
            } else if (strcmp(cmd, "i2c_scan") == 0) {
                String result_json = perform_i2c_scan();
                client->text("{\"type\":\"i2c\",\"devices\":" + result_json + "}");
            } else if (strcmp(cmd, "set_pwm") == 0) {
                uint32_t freq = doc["freq"];
                uint8_t  duty = doc["duty"];
                configure_pwm_generator(freq, duty);
                client->text("{\"type\":\"term\",\"text\":\"[PWM] Set to " + String(freq) + " Hz @ " + String(duty) + "% duty cycle.\\n> \"}");
            } else if (strcmp(cmd, "spi_flash") == 0) {
                char resp[256] = {0};
                if (spi_master_exec_command("flash probe", resp, sizeof(resp)) && strlen(resp) > 0) {
                    client->text("{\"type\":\"term\",\"text\":\"" + String(resp) + "\\n> \"}");
                } else {
                    PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_PIRATE_SPI_FLASH, 0 };
                    RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
                    client->text("{\"type\":\"term\",\"text\":\"[SPI Flash] Sent JEDEC ID Probe request to RP2040.\\n> \"}");
                }
            } else if (strcmp(cmd, "swd_scan") == 0) {
                run_onboard_swd_probe(nullptr, client);
            } else if (strcmp(cmd, "wifi_recon") == 0) {
                String reconJson = Esp32Wireless::WifiService::instance().runWifiReconJson();
                client->text("{\"type\":\"wifi_recon\",\"data\":" + reconJson + "}");
            } else if (strcmp(cmd, "wifi") == 0) {
                const char* sub = doc["args"] | "status";
                if (strcmp(sub, "status") == 0) {
                    auto rep = Esp32Wireless::WifiService::instance().getStatusReport();
                    JsonDocument resp;
                    resp["type"] = "response";
                    resp["command"] = "wifi";
                    resp["ok"] = true;
                    JsonObject d = resp["data"].to<JsonObject>();
                    d["state"] = Esp32Wireless::wifiStateToString(rep.state);
                    d["ssid"] = rep.ap_ssid;
                    d["ip"] = WiFi.softAPIP().toString();
                    d["channel"] = rep.channel;
                    d["clients"] = rep.station_count;
                    String resJson;
                    serializeJson(resp, resJson);
                    client->text(resJson);
                }
            } else if (strcmp(cmd, "term") == 0) {
                const char* txt = doc["text"];
                handle_terminal_cmd(txt, &Serial, client);
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(10);

    Serial.println("\r\n==================================================");
    Serial.println(" UDOO KEY ESP32 COPROCESSOR");
    Serial.println("==================================================");

    Serial.println("\r\n[BOOT] ESP32 initialization");
    // Factory Inter-MCU UART Link (IO22 RX / IO19 TX)
    RP2040_SERIAL.begin(1000000, SERIAL_8N1, ESP32_PIN_UART_RX, ESP32_PIN_UART_TX);
    Serial.println("[BOOT] UART transport ........ OK");
    Serial.printf("[BOOT] Reset reason .......... %d\r\n", (int)esp_reset_reason());

    // Allocate 64 KiB DMA/Logic Scope capture buffer from internal heap
    last_sample_buffer = (uint8_t*)malloc(MAX_SAMPLE_BUFFER_SIZE);
    if (last_sample_buffer) {
        memset(last_sample_buffer, 0, MAX_SAMPLE_BUFFER_SIZE);
        Serial.println("[BOOT] 64 KiB Logic Scope buffer ... OK");
    } else {
        Serial.println("[BOOT] 64 KiB buffer allocation failed!");
    }

    // Section 7: RP2040 Hardware Reset Control (ESP32 IO23)
    pinMode(ESP32_PIN_RPI_RESET, OUTPUT);
    digitalWrite(ESP32_PIN_RPI_RESET, HIGH); // Release RP2040 reset on boot

    // Section 8: Reserved RP2040 SWD debug/programming pins in Safe Input mode
    pinMode(ESP32_PIN_SWDIO, INPUT);
    pinMode(ESP32_PIN_SWCLK, INPUT);
    pinMode(ESP32_PIN_SWD_OE, INPUT);

    // Initialize Resource Manager
    Esp32Core::ResourceManager::instance();
    Serial.println("[BOOT] Resource manager ...... OK");

    // Initialize IR & nRF24
    ir_configure(25, 26, 38000, 33);
    Esp32Ir::IrService::instance().init(25, 26, 38000, 33);
    Serial.println("[BOOT] IR subsystem .......... OK");

    nrf24_init();
    Esp32Rf::Nrf24Service::instance().init();
    Serial.println("[BOOT] nRF24 subsystem ....... OK");

    // Initialize Wireless & Time Sync Service
    wireless_diag_init(&RP2040_SERIAL);

    // Initialize Default 1kHz 50% PWM Signal Output
    configure_pwm_generator(1000, 50);

    // Deterministic Wi-Fi Initialization via WifiService (Requirements 5, 8, 23)
    Serial.println("\r\n[WIFI] Initializing");
    Serial.println("[WIFI] Mode .................. AP");
    Serial.printf("[WIFI] SSID .................. %s\r\n", Esp32Wireless::WifiConfig::AP_SSID);
    Serial.printf("[WIFI] Channel ............... %u\r\n", Esp32Wireless::WifiConfig::AP_CHANNEL);

    bool ap_ok = Esp32Wireless::WifiService::instance().initialize();

    Serial.printf("[WIFI] AP IP ................. %s\r\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("[WIFI] MAC ................... %s\r\n", WiFi.softAPmacAddress().c_str());
    Serial.printf("[WIFI] AP start .............. %s\r\n", ap_ok ? "OK" : "FAILED");

    if (ap_ok) {
        // Register HTTP endpoints (Requirement 14)
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(200, "text/html", index_html);
        });

        server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
            auto rep = Esp32Wireless::WifiService::instance().getStatusReport();
            JsonDocument doc;
            doc["device"] = "UDOO KEY";
            doc["subsystem"] = "ESP32";
            doc["status"] = "OK";
            JsonObject wf = doc["wifi"].to<JsonObject>();
            wf["mode"] = "AP";
            wf["state"] = Esp32Wireless::wifiStateToString(rep.state);
            wf["ssid"] = rep.ap_ssid;
            wf["ip"] = WiFi.softAPIP().toString();
            wf["channel"] = rep.channel;
            wf["clients"] = rep.station_count;
            doc["uptime_ms"] = millis();
            doc["free_heap"] = ESP.getFreeHeap();

            String res;
            serializeJson(doc, res);
            request->send(200, "application/json", res);
        });

        server.on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
            auto rep = Esp32Wireless::WifiService::instance().getStatusReport();
            JsonDocument doc;
            doc["mode"] = "AP";
            doc["state"] = Esp32Wireless::wifiStateToString(rep.state);
            doc["ssid"] = rep.ap_ssid;
            doc["ip"] = WiFi.softAPIP().toString();
            doc["channel"] = rep.channel;
            doc["mac"] = WiFi.softAPmacAddress();
            doc["clients"] = rep.station_count;
            doc["max_clients"] = Esp32Wireless::WifiConfig::AP_MAX_CLIENTS;

            String res;
            serializeJson(doc, res);
            request->send(200, "application/json", res);
        });

        server.on("/api/system", HTTP_GET, [](AsyncWebServerRequest *request) {
            JsonDocument doc;
            doc["device"] = "UDOO KEY BASIC";
            doc["chip"] = "ESP32-D0WDQ6";
            doc["cpu_freq_mhz"] = getCpuFrequencyMhz();
            doc["free_heap"] = ESP.getFreeHeap();
            doc["flash_size_bytes"] = ESP.getFlashChipSize();
            doc["uptime_ms"] = millis();

            String res;
            serializeJson(doc, res);
            request->send(200, "application/json", res);
        });

        server.on("/api/capabilities", HTTP_GET, [](AsyncWebServerRequest *request) {
            JsonDocument doc;
            doc["wifi"] = true;
            doc["ble"] = true;
            doc["ir"] = true;
            doc["nrf24"] = true;
            doc["logic_scope"] = true;
            doc["bit_pirate"] = true;
            doc["swd"] = true;
            doc["uart"] = true;
            doc["spi"] = true;
            doc["i2c"] = true;

            String res;
            serializeJson(doc, res);
            request->send(200, "application/json", res);
        });

        server.on("/api/logic/status", HTTP_GET, [](AsyncWebServerRequest *request) {
            JsonDocument doc;
            doc["state"] = (last_rp2040_status.acquisition_state == 1) ? "ARMED" :
                           (last_rp2040_status.acquisition_state == 2) ? "TRIGGERED" :
                           (last_rp2040_status.acquisition_state == 3) ? "READY" : "IDLE";
            doc["sample_rate_hz"] = last_capture_hdr.sample_rate_hz;
            doc["total_samples"]  = last_capture_hdr.total_samples;
            doc["trigger_offset"] = last_capture_hdr.trigger_offset;
            doc["capture_id"]     = last_capture_hdr.capture_id;
            doc["rp2040_online"]  = s_rp2040_online;
            String res;
            serializeJson(doc, res);
            request->send(200, "application/json", res);
        });

        server.on("/api/logic/config", HTTP_GET, [](AsyncWebServerRequest *request) {
            JsonDocument doc;
            doc["sample_rate_hz"] = last_capture_hdr.sample_rate_hz;
            doc["total_samples"]  = last_capture_hdr.total_samples;
            doc["channels"]       = 8;
            doc["trigger_offset"] = last_capture_hdr.trigger_offset;
            String res;
            serializeJson(doc, res);
            request->send(200, "application/json", res);
        });

        server.on("/api/logic/arm", HTTP_POST, [](AsyncWebServerRequest *request) {
            send_instrument_packet(PKT_TYPE_START_CAPTURE, nullptr, 0);
            PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_LA_START, 0 };
            RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
            request->send(200, "application/json", "{\"status\":\"ARMED\",\"ok\":true}");
        });

        server.on("/api/logic/single", HTTP_POST, [](AsyncWebServerRequest *request) {
            send_instrument_packet(PKT_TYPE_START_CAPTURE, nullptr, 0);
            PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_LA_START, 0 };
            RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
            request->send(200, "application/json", "{\"status\":\"ARMED\",\"ok\":true}");
        });

        server.on("/api/logic/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
            send_instrument_packet(PKT_TYPE_STOP_CAPTURE, nullptr, 0);
            PacketHeader hdr = { PROTOCOL_SYNC_BYTE, PKT_LA_STOP, 0 };
            RP2040_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
            request->send(200, "application/json", "{\"status\":\"STOPPED\",\"ok\":true}");
        });

        server.on("/api/logic/capture", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (last_buffer_len == 0) {
                request->send(404, "application/json", "{\"error\":\"No capture available\"}");
                return;
            }
            if (request->hasParam("format") && request->getParam("format")->value() == "json") {
                JsonDocument doc;
                doc["capture_id"]     = last_capture_hdr.capture_id;
                doc["sample_rate_hz"] = last_capture_hdr.sample_rate_hz;
                doc["total_samples"]  = last_buffer_len;
                doc["trigger_offset"] = last_capture_hdr.trigger_offset;
                String res;
                serializeJson(doc, res);
                request->send(200, "application/json", res);
            } else {
                AsyncResponseStream *response = request->beginResponseStream("application/octet-stream");
                response->write((const uint8_t*)&last_capture_hdr, sizeof(WebLogicCaptureHeader));
                response->write((const uint8_t*)last_sample_buffer, last_buffer_len);
                request->send(response);
            }
        });

        server.on("/api/adc", HTTP_GET, [](AsyncWebServerRequest *request) {
            JsonDocument doc;
            doc["adc0_mv"] = last_rp2040_status.adc0_mv;
            doc["adc1_mv"] = last_rp2040_status.adc1_mv;
            doc["adc2_mv"] = last_rp2040_status.adc2_mv;
            String res;
            serializeJson(doc, res);
            request->send(200, "application/json", res);
        });

        server.onNotFound([](AsyncWebServerRequest *request) {
            String url = request->url();
            if (url.indexOf("generate_204") >= 0 || url.indexOf("hotspot-detect") >= 0 ||
                url.indexOf("canonical.html") >= 0 || url.indexOf("connecttest.txt") >= 0 ||
                url.indexOf("ncsi.txt") >= 0) {
                request->redirect("http://192.168.4.1/");
            } else {
                request->send(200, "text/html", index_html);
            }
        });

        ws.onEvent(onWebSocketEvent);
        server.addHandler(&ws);
        server.begin();

        Serial.println("[WIFI] HTTP server ........... OK");
        Serial.println("[WIFI] WebSocket ............. OK");
    } else {
        Serial.println("[WIFI] HTTP server ........... SKIPPED (AP FAILED)");
        Serial.println("[WIFI] WebSocket ............. SKIPPED (AP FAILED)");
    }

    // BLE Subsystem: On-demand initialization to avoid RF coexistence contention during Wi-Fi AP boot
    Serial.println("\r\n[BLE] Subsystem .............. ON-DEMAND");

    // Stream initial Wi-Fi telemetry to RP2040 so desktop companion knows the AP is active
    wireless_diag_stream_wifi_telemetry();

    Serial.println("\r\n==================================================");
    Serial.println(" UDOO KEY ESP32 READY");
    Serial.println("==================================================");
    Serial.print(Esp32Core::ModeManager::instance().prompt());
}

void loop() {
    ws.cleanupClients();

    // Service Wi-Fi Promiscuous Sniffer FreeRTOS Queue
    Esp32Wireless::WifiService::instance().processSnifferQueue();

    // Service Wi-Fi Health Check / Watchdog (Requirement 24)
    Esp32Wireless::WifiService::instance().healthCheck();

    // Service User UART Monitor (if active)
    if (Esp32Bus::UartService::instance().isMonitoring()) {
        Esp32Bus::UartService::instance().pollMonitor(Serial);
    }

    // Process Wireless Diagnostics & Background Time Sync
    wireless_diag_task();

    // Process serial terminal input from USB CDC
    process_usb_serial();

    // Periodic background Wi-Fi diagnostic telemetry (every 20s)
    if (millis() - s_last_wifi_scan_ms > 20000) {
        s_last_wifi_scan_ms = millis();
        wireless_diag_stream_wifi_telemetry();
    }

    // 0. Optional High-Speed Transport: Check hardware DATA_READY line from RP2040 (only if SPI active)
    if (spi_master_transport_is_active() && spi_master_is_data_ready()) {
        SpiCaptureInfoPayload info;
        if (spi_master_fetch_complete_capture(last_sample_buffer, MAX_SAMPLE_BUFFER_SIZE, info)) {
            last_buffer_len = info.total_samples;

            // Broadcast verified binary sample buffer to all connected phone/tablet web browsers!
            ws.binaryAll((char*)last_sample_buffer, last_buffer_len);

            String stats = "\\n[Optional SPI 10MHz] Ingested Capture #" + String(info.capture_id) + ": " +
                           String(info.total_samples) + " samples @ " +
                           String(info.sample_rate_hz / 1000000.0, 1) + " MHz (Blocks + Whole CRC-16 OK)\\n> ";
            ws.textAll("{\"type\":\"term\",\"text\":\"" + stats + "\"}");
        }
    }

    // 1. Transport-Neutral Binary Protocol Packets (InstrumentProtocol v1.0)
    if (RP2040_SERIAL.available() >= (int)sizeof(InstrumentPacketHeader) &&
        RP2040_SERIAL.peek() == (INSTRUMENT_PROTO_MAGIC & 0xFF)) {
        InstrumentPacketHeader ihdr;
        RP2040_SERIAL.readBytes((char*)&ihdr, sizeof(ihdr));
        if (ihdr.magic == INSTRUMENT_PROTO_MAGIC) {
            uint16_t plen = ihdr.payload_len;
            if (ihdr.type == PKT_TYPE_TIME_SYNC_REQ) {
                InstrumentTimeSyncReqPayload sync_req;
                if (plen >= sizeof(sync_req)) {
                    RP2040_SERIAL.readBytes((char*)&sync_req, sizeof(sync_req));
                }
                InstrumentTimeSyncRespPayload sync_resp;
                sync_resp.host_request_id     = sync_req.host_request_id;
                sync_resp.host_timestamp_us   = sync_req.host_timestamp_us;
                sync_resp.device_timestamp_us = (uint64_t)esp_timer_get_time();
                sync_resp.clock_drift_ppm     = 0;
                send_instrument_packet(PKT_TYPE_TIME_SYNC_RESP, &sync_resp, sizeof(sync_resp));
            } else if (ihdr.type == PKT_TYPE_CAPTURE_INFO_RESP && plen <= sizeof(InstrumentCaptureInfoPayload)) {
                InstrumentCaptureInfoPayload info;
                RP2040_SERIAL.readBytes((char*)&info, plen);
                last_capture_hdr.magic          = 0x53434F50;
                last_capture_hdr.version        = 1;
                last_capture_hdr.channel_count  = 8;
                last_capture_hdr.flags          = 1;
                last_capture_hdr.sample_rate_hz = info.sample_rate_hz;
                last_capture_hdr.total_samples  = info.total_samples;
                last_capture_hdr.trigger_offset = info.trigger_offset;
                last_capture_hdr.pre_samples    = info.pre_samples;
                last_capture_hdr.post_samples   = info.post_samples;
                last_capture_hdr.capture_id     = info.capture_id;
                last_buffer_len                 = 0;
            } else if (ihdr.type == PKT_TYPE_CAPTURE_DATA_CHUNK && plen <= sizeof(InstrumentChunkDataPayload)) {
                InstrumentChunkDataPayload chunk;
                RP2040_SERIAL.readBytes((char*)&chunk, plen);
                uint32_t dest_offset = chunk.chunk_index * INSTRUMENT_CHUNK_SIZE;
                if (dest_offset + chunk.chunk_len <= MAX_SAMPLE_BUFFER_SIZE) {
                    memcpy(last_sample_buffer + dest_offset, chunk.data, chunk.chunk_len);
                    if (dest_offset + chunk.chunk_len > last_buffer_len) {
                        last_buffer_len = dest_offset + chunk.chunk_len;
                    }
                    if (chunk.chunk_index + 1 == chunk.total_chunks) {
                        last_capture_hdr.total_samples = last_buffer_len;
                        size_t total_payload = sizeof(WebLogicCaptureHeader) + last_buffer_len;
                        uint8_t* tx_buf = (uint8_t*)malloc(total_payload);
                        if (tx_buf) {
                            memcpy(tx_buf, &last_capture_hdr, sizeof(WebLogicCaptureHeader));
                            memcpy(tx_buf + sizeof(WebLogicCaptureHeader), last_sample_buffer, last_buffer_len);
                            ws.binaryAll((char*)tx_buf, total_payload);
                            free(tx_buf);
                        } else {
                            ws.binaryAll((char*)last_sample_buffer, last_buffer_len);
                        }

                        JsonDocument doc;
                        doc["type"]           = "capture_ready";
                        doc["capture_id"]     = last_capture_hdr.capture_id;
                        doc["rate"]           = last_capture_hdr.sample_rate_hz;
                        doc["samples"]        = last_buffer_len;
                        doc["trigger_offset"] = last_capture_hdr.trigger_offset;
                        String js;
                        serializeJson(doc, js);
                        ws.textAll(js);
                    }
                }
            } else if (ihdr.type == PKT_TYPE_STATUS_RESP && plen <= sizeof(InstrumentStatusPayload)) {
                RP2040_SERIAL.readBytes((char*)&last_rp2040_status, plen);
                s_rp2040_online = true;
                s_last_rp2040_ping_ms = millis();
            } else if (ihdr.type == PKT_TYPE_PONG) {
                s_rp2040_online = true;
                s_last_rp2040_ping_ms = millis();
            } else if (ihdr.type == PKT_TYPE_TRIGGER_EVENT) {
                if (plen > 0) {
                    char dummy[16];
                    RP2040_SERIAL.readBytes(dummy, plen > sizeof(dummy) ? sizeof(dummy) : plen);
                }
                JsonDocument doc;
                doc["type"] = "trigger_fired";
                String js;
                serializeJson(doc, js);
                ws.textAll(js);
            } else if (ihdr.type == PKT_TYPE_IR_CONFIG && plen <= sizeof(InstrumentIrConfigPayload)) {
                InstrumentIrConfigPayload cfg;
                RP2040_SERIAL.readBytes((char*)&cfg, plen);
                ir_configure(cfg.tx_pin, cfg.rx_pin, cfg.carrier_freq_hz, cfg.duty_percent);
                InstrumentPacketHeader ackHdr = { INSTRUMENT_PROTO_MAGIC, INSTRUMENT_PROTO_VERSION, PKT_TYPE_CONFIG_ACK, ihdr.sequence, 0, 0 };
                RP2040_SERIAL.write((uint8_t*)&ackHdr, sizeof(ackHdr));
            } else if (ihdr.type == PKT_TYPE_IR_TX && plen <= sizeof(InstrumentIrTxPayload)) {
                InstrumentIrTxPayload tx;
                RP2040_SERIAL.readBytes((char*)&tx, plen);
                if (tx.protocol == IR_PROTO_NEC) {
                    ir_send_nec(tx.address, tx.command);
                } else if (tx.protocol == IR_PROTO_RAW && tx.raw_len > 0) {
                    ir_send_raw(tx.raw_timings_us, tx.raw_len, s_ir_carrier_freq_hz);
                }
                InstrumentPacketHeader ackHdr = { INSTRUMENT_PROTO_MAGIC, INSTRUMENT_PROTO_VERSION, PKT_TYPE_CONFIG_ACK, ihdr.sequence, 0, 0 };
                RP2040_SERIAL.write((uint8_t*)&ackHdr, sizeof(ackHdr));
            } else if (ihdr.type == PKT_TYPE_IR_RX_CONFIG && plen <= sizeof(InstrumentIrRxConfigPayload)) {
                InstrumentIrRxConfigPayload rxcfg;
                RP2040_SERIAL.readBytes((char*)&rxcfg, plen);
                s_ir_rx_enabled = (rxcfg.enabled != 0);
                InstrumentPacketHeader ackHdr = { INSTRUMENT_PROTO_MAGIC, INSTRUMENT_PROTO_VERSION, PKT_TYPE_CONFIG_ACK, ihdr.sequence, 0, 0 };
                RP2040_SERIAL.write((uint8_t*)&ackHdr, sizeof(ackHdr));
            } else if (ihdr.type == PKT_TYPE_NRF24_CONFIG && plen <= sizeof(InstrumentNrf24ConfigPayload)) {
                InstrumentNrf24ConfigPayload ncfg;
                RP2040_SERIAL.readBytes((char*)&ncfg, plen);
                s_nrf24_channel = ncfg.channel;
                nrf24_write_reg(0x05, s_nrf24_channel);
                InstrumentPacketHeader ackHdr = { INSTRUMENT_PROTO_MAGIC, INSTRUMENT_PROTO_VERSION, PKT_TYPE_CONFIG_ACK, ihdr.sequence, 0, 0 };
                RP2040_SERIAL.write((uint8_t*)&ackHdr, sizeof(ackHdr));
            } else if (ihdr.type == PKT_TYPE_NRF24_TX && plen <= sizeof(InstrumentNrf24TxPayload)) {
                InstrumentNrf24TxPayload ntx;
                RP2040_SERIAL.readBytes((char*)&ntx, plen);
                InstrumentNrf24TxStatusPayload st;
                st.status = s_nrf24_connected ? 0 : 2;
                st.retries_count = 0;
                InstrumentPacketHeader repHdr = { INSTRUMENT_PROTO_MAGIC, INSTRUMENT_PROTO_VERSION, PKT_TYPE_NRF24_TX_STATUS, ihdr.sequence, sizeof(st), instrument_crc16((const uint8_t*)&st, sizeof(st)) };
                RP2040_SERIAL.write((uint8_t*)&repHdr, sizeof(repHdr));
                RP2040_SERIAL.write((uint8_t*)&st, sizeof(st));
            } else if (ihdr.type == PKT_TYPE_NRF24_RX_CONFIG && plen <= sizeof(InstrumentNrf24RxConfigPayload)) {
                InstrumentNrf24RxConfigPayload nrxcfg;
                RP2040_SERIAL.readBytes((char*)&nrxcfg, plen);
                InstrumentPacketHeader ackHdr = { INSTRUMENT_PROTO_MAGIC, INSTRUMENT_PROTO_VERSION, PKT_TYPE_CONFIG_ACK, ihdr.sequence, 0, 0 };
                RP2040_SERIAL.write((uint8_t*)&ackHdr, sizeof(ackHdr));
            } else if (ihdr.type == PKT_TYPE_NRF24_STATUS) {
                if (plen > 0) {
                    char dummy[32];
                    RP2040_SERIAL.readBytes(dummy, plen > sizeof(dummy) ? sizeof(dummy) : plen);
                }
                InstrumentNrf24StatusPayload st;
                memset(&st, 0, sizeof(st));
                st.is_connected = s_nrf24_connected ? 1 : 0;
                st.carrier_detected = 0;
                st.rx_fifo_empty = 1;
                st.tx_fifo_full = 0;
                st.active_channel = s_nrf24_channel;
                InstrumentPacketHeader repHdr = { INSTRUMENT_PROTO_MAGIC, INSTRUMENT_PROTO_VERSION, PKT_TYPE_NRF24_STATUS, ihdr.sequence, sizeof(st), instrument_crc16((const uint8_t*)&st, sizeof(st)) };
                RP2040_SERIAL.write((uint8_t*)&repHdr, sizeof(repHdr));
                RP2040_SERIAL.write((uint8_t*)&st, sizeof(st));
            } else if (ihdr.type == PKT_TYPE_RADIO_CMD && plen <= sizeof(InstrumentRadioCmdPayload)) {
                InstrumentRadioCmdPayload rcmd;
                RP2040_SERIAL.readBytes((char*)&rcmd, plen);
                wireless_diag_process_radio_cmd(rcmd);
            } else if (ihdr.type == PKT_TYPE_WIFI_SNIFF_CONFIG && plen <= sizeof(InstrumentWifiSniffConfigPayload)) {
                InstrumentWifiSniffConfigPayload scfg;
                RP2040_SERIAL.readBytes((char*)&scfg, plen);
                if (scfg.enabled) {
                    Esp32Wireless::WifiService::instance().startSniffer(scfg.channel, static_cast<Esp32Wireless::SniffFilter>(scfg.filter_type));
                } else {
                    Esp32Wireless::WifiService::instance().stopSniffer();
                }
                InstrumentPacketHeader ackHdr = { INSTRUMENT_PROTO_MAGIC, INSTRUMENT_PROTO_VERSION, PKT_TYPE_CONFIG_ACK, ihdr.sequence, 0, 0 };
                RP2040_SERIAL.write((uint8_t*)&ackHdr, sizeof(ackHdr));
            } else if (ihdr.type == PKT_TYPE_WIFI_SNIFF_STATUS) {
                if (plen > 0) {
                    char dummy[16];
                    RP2040_SERIAL.readBytes(dummy, plen > sizeof(dummy) ? sizeof(dummy) : plen);
                }
                InstrumentWifiSniffStatusPayload sst;
                memset(&sst, 0, sizeof(sst));
                sst.active = Esp32Wireless::WifiService::instance().isSnifferActive() ? 1 : 0;
                sst.channel = 1;
                sst.filter_type = 0;
                sst.captured_count = Esp32Packet::PcapService::instance().packetCount();
                sst.dropped_count = Esp32Packet::PcapService::instance().droppedCount();
                InstrumentPacketHeader repHdr = { INSTRUMENT_PROTO_MAGIC, INSTRUMENT_PROTO_VERSION, PKT_TYPE_WIFI_SNIFF_STATUS, ihdr.sequence, sizeof(sst), instrument_crc16((const uint8_t*)&sst, sizeof(sst)) };
                RP2040_SERIAL.write((uint8_t*)&repHdr, sizeof(repHdr));
                RP2040_SERIAL.write((uint8_t*)&sst, sizeof(sst));
            } else if (ihdr.type == PKT_TYPE_PCAP_DUMP_REQ) {
                if (plen > 0) {
                    char dummy[16];
                    RP2040_SERIAL.readBytes(dummy, plen > sizeof(dummy) ? sizeof(dummy) : plen);
                }
                Esp32Packet::PcapService::instance().dumpBinary(RP2040_SERIAL);
            } else if (plen > 0) {
                char dummy[64];
                while (plen > 0) {
                    uint16_t tr = plen > sizeof(dummy) ? sizeof(dummy) : plen;
                    RP2040_SERIAL.readBytes(dummy, tr);
                    plen -= tr;
                }
            }
        }
    }
    // 2. Legacy Transport Packets (0xAA)
    else if (RP2040_SERIAL.available() >= (int)sizeof(PacketHeader) && RP2040_SERIAL.peek() == PROTOCOL_SYNC_BYTE) {
        PacketHeader hdr;
        RP2040_SERIAL.readBytes((char*)&hdr, sizeof(hdr));

        if (hdr.type == PKT_LA_STREAM) {
            uint32_t len = hdr.payload_len;
            if (len > MAX_SAMPLE_BUFFER_SIZE) len = MAX_SAMPLE_BUFFER_SIZE;

            RP2040_SERIAL.readBytes((char*)last_sample_buffer, len);
            last_buffer_len = len;
            last_capture_hdr.total_samples = len;
            last_capture_hdr.capture_id++;

            size_t total_payload = sizeof(WebLogicCaptureHeader) + last_buffer_len;
            uint8_t* tx_buf = (uint8_t*)malloc(total_payload);
            if (tx_buf) {
                memcpy(tx_buf, &last_capture_hdr, sizeof(WebLogicCaptureHeader));
                memcpy(tx_buf + sizeof(WebLogicCaptureHeader), last_sample_buffer, last_buffer_len);
                ws.binaryAll((char*)tx_buf, total_payload);
                free(tx_buf);
            } else {
                ws.binaryAll((char*)last_sample_buffer, last_buffer_len);
            }
        }
    }

    // Periodic RP2040 keepalive & telemetry poll
    static uint32_t s_last_rp2040_poll_ms = 0;
    if (millis() - s_last_rp2040_poll_ms > 3000) {
        s_last_rp2040_poll_ms = millis();
        send_instrument_packet(PKT_TYPE_PING, nullptr, 0);
        send_instrument_packet(PKT_TYPE_GET_STATUS, nullptr, 0);
        if (millis() - s_last_rp2040_ping_ms > 8000) {
            s_rp2040_online = false;
        }
    } 
    // 3. Read ASCII Bus Pirate CLI response text from RP2040 and forward to Web Terminal & USB Serial!
    else if (RP2040_SERIAL.available() && RP2040_SERIAL.peek() != PROTOCOL_SYNC_BYTE && RP2040_SERIAL.peek() != (INSTRUMENT_PROTO_MAGIC & 0xFF)) {
        String uart_text = "";
        while (RP2040_SERIAL.available() && RP2040_SERIAL.peek() != PROTOCOL_SYNC_BYTE && uart_text.length() < 128) {
            char c = RP2040_SERIAL.read();
            Serial.write(c); // Echo directly to PC USB console (COM6)
            if (c == '\r') continue; // Clean carriage return
            if (c == '\n') uart_text += "\\n";
            else if (c == '\"') uart_text += "\\\"";
            else if (c == '\\') uart_text += "\\\\";
            else uart_text += c;
        }
        if (uart_text.length() > 0) {
            ws.textAll("{\"type\":\"term\",\"text\":\"" + uart_text + "\"}");
        }
    }
}
