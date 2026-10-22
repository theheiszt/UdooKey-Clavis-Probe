#include "nrf24_service.h"
#include "../core/resource_manager.h"
#include "../packet/pcap_service.h"
#include <cstring>
#include <strings.h>

namespace Esp32Rf {

Nrf24Service& Nrf24Service::instance() {
    static Nrf24Service s_instance;
    return s_instance;
}

Nrf24Service::Nrf24Service() = default;

uint8_t Nrf24Service::spiXfer(uint8_t byteOut) {
    uint8_t byteIn = 0;
    for (int i = 7; i >= 0; i--) {
        digitalWrite(m_mosiPin, (byteOut >> i) & 1);
        digitalWrite(m_sckPin, HIGH);
        delayMicroseconds(1);
        byteIn = (byteIn << 1) | (digitalRead(m_misoPin) & 1);
        digitalWrite(m_sckPin, LOW);
        delayMicroseconds(1);
    }
    return byteIn;
}

uint8_t Nrf24Service::readReg(uint8_t reg) {
    digitalWrite(m_csnPin, LOW);
    spiXfer(reg & 0x1F);
    uint8_t val = spiXfer(0xFF);
    digitalWrite(m_csnPin, HIGH);
    return val;
}

void Nrf24Service::writeReg(uint8_t reg, uint8_t val) {
    digitalWrite(m_csnPin, LOW);
    spiXfer((reg & 0x1F) | 0x20);
    spiXfer(val);
    digitalWrite(m_csnPin, HIGH);
}

void Nrf24Service::writeBuf(uint8_t reg, const uint8_t* buf, uint8_t len) {
    digitalWrite(m_csnPin, LOW);
    spiXfer((reg & 0x1F) | 0x20);
    for (uint8_t i = 0; i < len; ++i) spiXfer(buf[i]);
    digitalWrite(m_csnPin, HIGH);
}

void Nrf24Service::readBuf(uint8_t reg, uint8_t* buf, uint8_t len) {
    digitalWrite(m_csnPin, LOW);
    spiXfer(reg & 0x1F);
    for (uint8_t i = 0; i < len; ++i) buf[i] = spiXfer(0xFF);
    digitalWrite(m_csnPin, HIGH);
}

bool Nrf24Service::init() {
    auto& rm = Esp32Core::ResourceManager::instance();
    rm.acquire(Esp32Core::Resource::NRF24_RADIO, Esp32Core::Owner::NRF24_SERVICE);

    pinMode(m_cePin, OUTPUT);
    pinMode(m_csnPin, OUTPUT);
    pinMode(m_sckPin, OUTPUT);
    pinMode(m_mosiPin, OUTPUT);
    pinMode(m_misoPin, INPUT);

    digitalWrite(m_cePin, LOW);
    digitalWrite(m_csnPin, HIGH);
    digitalWrite(m_sckPin, LOW);

    // Write to RF_CH (reg 0x05) to test presence
    writeReg(0x05, 0x4C);
    uint8_t val = readReg(0x05);
    m_connected = (val == 0x4C);

    if (m_connected) {
        // Basic configuration: 1Mbps, 0dBm, 16-bit CRC
        writeReg(0x00, 0x0C); // CONFIG: EN_CRC, CRCO (16-bit)
        writeReg(0x01, 0x3F); // EN_AA: All pipes auto-ack
        writeReg(0x02, 0x03); // EN_RXADDR: Pipes 0, 1 enabled
        writeReg(0x03, 0x03); // SETUP_AW: 5 bytes
        writeReg(0x04, 0x03); // SETUP_RETR: 250us, 3 retries
        writeReg(0x06, 0x06); // RF_SETUP: 1Mbps, 0dBm
        writeReg(0x05, m_channel);
    }
    return m_connected;
}

void Nrf24Service::deinit() {
    digitalWrite(m_cePin, LOW);
    digitalWrite(m_csnPin, HIGH);
    auto& rm = Esp32Core::ResourceManager::instance();
    rm.release(Esp32Core::Resource::NRF24_RADIO, Esp32Core::Owner::NRF24_SERVICE);
    m_connected = false;
    m_sniffing = false;
}

void Nrf24Service::printStatus(Print& out) {
    if (!m_connected) init();

    out.println("\r\n=== nRF24L01+ 2.4 GHz RF SUBSYSTEM ===");
    out.printf("  Hardware Module : %s\r\n", m_connected ? "DETECTED / OK (UEXT P3)" : "NOT CONNECTED");
    out.printf("  Active Channel  : %u (2.%03u GHz)\r\n", m_channel, 400 + m_channel);
    out.printf("  RF Data Rate    : 1 Mbps\r\n");
    out.printf("  RF Output Power : 0 dBm (Max)\r\n");
    out.printf("  CRC Encoding    : 16-bit CRC\r\n");
    out.printf("  Address Width   : 5 Bytes\r\n");
    out.printf("  Sniffer Mode    : %s\r\n", m_sniffing ? "ACTIVE" : "IDLE");
    out.printf("  UEXT Connections: CE=IO%d, CSN=IO%d, SCK=IO%d, MOSI=IO%d, MISO=IO%d\r\n",
               m_cePin, m_csnPin, m_sckPin, m_mosiPin, m_misoPin);
    out.println("======================================");
}

bool Nrf24Service::setChannel(uint8_t channel, Print& out) {
    if (channel > 125) {
        out.println("Error: Channel must be between 0 and 125.");
        return false;
    }
    m_channel = channel;
    if (m_connected) {
        writeReg(0x05, m_channel);
    }
    out.printf("nRF24 Channel set to %u (2.%03u GHz)\r\n", m_channel, 400 + m_channel);
    return true;
}

bool Nrf24Service::transmitPacket(const uint8_t* addr, const uint8_t* data, size_t len, Print& out) {
    if (!m_connected) init();
    if (!m_connected) {
        out.println("Error: nRF24 module not detected on UEXT connector.");
        return false;
    }

    uint8_t defaultAddr[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7};
    const uint8_t* txAddr = addr ? addr : defaultAddr;

    digitalWrite(m_cePin, LOW);
    writeBuf(0x10, txAddr, 5); // TX_ADDR
    writeBuf(0x0A, txAddr, 5); // RX_ADDR_P0 for ACK

    // Write payload (W_TX_PAYLOAD 0xA0)
    digitalWrite(m_csnPin, LOW);
    spiXfer(0xA0);
    for (size_t i = 0; i < len && i < 32; ++i) spiXfer(data[i]);
    digitalWrite(m_csnPin, HIGH);

    // Pulse CE to transmit
    digitalWrite(m_cePin, HIGH);
    delayMicroseconds(15);
    digitalWrite(m_cePin, LOW);

    out.printf("TX transmitted %u bytes to %02X:%02X:%02X:%02X:%02X\r\n",
               (unsigned int)len, txAddr[0], txAddr[1], txAddr[2], txAddr[3], txAddr[4]);
    return true;
}

void Nrf24Service::receivePacket(Print& out) {
    if (!m_connected) init();
    if (!m_connected) {
        out.println("Error: nRF24 module not detected.");
        return;
    }

    uint8_t status = readReg(0x07); // STATUS
    if (status & 0x40) { // RX_DR
        uint8_t pipe = (status >> 1) & 0x07;
        uint8_t rxData[32] = {0};
        readBuf(0x61, rxData, 32); // R_RX_PAYLOAD
        writeReg(0x07, 0x40);      // Clear RX_DR flag

        out.printf("RX Packet (Pipe %u): ", pipe);
        for (int i = 0; i < 32; ++i) out.printf("%02X ", rxData[i]);
        out.println();

        // Feed PCAP service
        Esp32Packet::PacketRecord rec;
        rec.timestamp_us = esp_timer_get_time();
        rec.source = static_cast<uint8_t>(Esp32Packet::CaptureSource::NRF24);
        rec.channel = m_channel;
        rec.rssi = -60;
        rec.captured_length = 32;
        rec.original_length = 32;
        rec.link_type = Esp32Packet::LINKTYPE_USER0;
        memcpy(rec.data, rxData, 32);
        Esp32Packet::PcapService::instance().pushPacket(rec);
    } else {
        out.println("RX FIFO empty. No packets received.");
    }
}

void Nrf24Service::startSniff(uint8_t channel, Print& out) {
    setChannel(channel, out);
    m_sniffing = true;
    Esp32Packet::PcapService::instance().startCapture(Esp32Packet::LINKTYPE_USER0);
    out.println("nRF24 Packet Sniffer STARTED.");
}

void Nrf24Service::stopSniff(Print& out) {
    m_sniffing = false;
    out.println("nRF24 Packet Sniffer STOPPED.");
}

void Nrf24Service::executeCommand(const char* args, Print& out) {
    while (*args && isspace(*args)) args++;
    if (!*args || strcasecmp(args, "status") == 0) {
        printStatus(out);
        return;
    }

    char cmd[16] = {0};
    int n = 0;
    while (*args && !isspace(*args) && n < 15) {
        cmd[n++] = *args++;
    }
    while (*args && isspace(*args)) args++;

    if (strcasecmp(cmd, "init") == 0) {
        bool ok = init();
        out.printf("nRF24 initialization %s.\r\n", ok ? "SUCCESSFUL" : "FAILED (No radio on UEXT)");
    } else if (strcasecmp(cmd, "ch") == 0 || strcasecmp(cmd, "channel") == 0) {
        uint8_t c = atoi(args);
        setChannel(c, out);
    } else if (strcasecmp(cmd, "rx") == 0) {
        receivePacket(out);
    } else if (strcasecmp(cmd, "tx") == 0) {
        uint8_t dummy[4] = {0xAA, 0x55, 0x12, 0x34};
        transmitPacket(nullptr, dummy, 4, out);
    } else if (strcasecmp(cmd, "sniff") == 0) {
        if (strcasecmp(args, "stop") == 0) stopSniff(out);
        else startSniff(m_channel, out);
    } else {
        out.println("Available commands: nrf24 status | nrf24 init | nrf24 ch <0-125> | nrf24 rx | nrf24 tx | nrf24 sniff [stop]");
    }
}

} // namespace Esp32Rf
