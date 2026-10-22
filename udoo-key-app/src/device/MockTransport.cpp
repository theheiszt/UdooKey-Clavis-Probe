#include "MockTransport.h"
#include "Logger.h"
#include <cmath>

namespace UdooKey {

MockTransport::MockTransport(QObject* parent)
    : ITransport(parent) {
}

bool MockTransport::open() {
    m_open = true;
    LOG_INFO("MockTransport opened");
    emit connected();
    return true;
}

void MockTransport::close() {
    if (m_open) {
        m_open = false;
        LOG_INFO("MockTransport closed");
        emit disconnected();
    }
}

bool MockTransport::isOpen() const {
    return m_open;
}

qint64 MockTransport::write(const QByteArray& data) {
    if (!m_open) return -1;
    handleCommand(data);
    return data.size();
}

qint64 MockTransport::write(const char* data, qint64 maxSize) {
    if (!m_open) return -1;
    QByteArray ba(data, static_cast<int>(maxSize));
    handleCommand(ba);
    return maxSize;
}

QByteArray MockTransport::readAll() {
    QByteArray ret = m_rxBuffer;
    m_rxBuffer.clear();
    return ret;
}

qint64 MockTransport::bytesAvailable() const {
    return m_rxBuffer.size();
}

void MockTransport::injectIncomingData(const QByteArray& data) {
    if (!m_open) return;
    m_rxBuffer.append(data);
    emit dataReady(data);
}

void MockTransport::generateSyntheticCapture(uint32_t sampleCount) {
    QByteArray payload;
    payload.reserve(4 + sampleCount);

    // 0x5A Framing header: [0x5A] [Length MSB] [Length MID] [Length LSB]
    payload.append(static_cast<char>(0x5A));
    payload.append(static_cast<char>((sampleCount >> 16) & 0xFF));
    payload.append(static_cast<char>((sampleCount >> 8) & 0xFF));
    payload.append(static_cast<char>(sampleCount & 0xFF));

    // Synthesize realistic digital waveforms across 8 channels:
    // CH0: 1 kHz Clock toggle every 50 samples
    // CH1: 500 Hz Clock toggle every 100 samples
    // CH2: UART TX pattern (8N1 byte sequence)
    // CH3: SPI CS# (active low frames)
    // CH4: SPI SCK (clock bursts)
    // CH5: SPI MOSI (data stream)
    // CH6: I2C SCL
    // CH7: I2C SDA
    for (uint32_t i = 0; i < sampleCount; ++i) {
        uint8_t byte = 0;

        // CH0: Clock 1
        if ((i / 50) % 2) byte |= (1 << 0);

        // CH1: Clock 2
        if ((i / 100) % 2) byte |= (1 << 1);

        // CH2: Periodic UART-like pulses
        if ((i / 20) % 10 < 5) byte |= (1 << 2);

        // CH3: SPI CS# (low during active transaction)
        bool cs = !((i % 1000) < 600);
        if (cs) byte |= (1 << 3);

        // CH4: SPI SCK (only toggling when CS is low)
        if (!cs && ((i / 4) % 2)) byte |= (1 << 4);

        // CH5: SPI MOSI
        if (!cs && ((i / 8) % 2)) byte |= (1 << 5);

        // CH6: I2C SCL
        if ((i / 16) % 2) byte |= (1 << 6);

        // CH7: I2C SDA
        if ((i / 32) % 2) byte |= (1 << 7);

        payload.append(static_cast<char>(byte));
    }

    injectIncomingData(payload);
}

void MockTransport::handleCommand(const QByteArray& cmd) {
    // 1. SUMP ID command: 0x02 -> "1ALS"
    if (cmd.contains('\x02')) {
        QByteArray resp("1ALS");
        injectIncomingData(resp);
        return;
    }

    // 2. SUMP Metadata command: 0x04 -> standard SUMP metadata
    if (cmd.contains('\x04')) {
        QByteArray resp;
        // Key 0x01: Name string
        resp.append(static_cast<char>(0x01));
        resp.append("UDOO KEY Logic Analyzer");
        resp.append(static_cast<char>(0x00));

        // Key 0x20: Sample memory size (65536 bytes) -> 4 bytes big endian
        resp.append(static_cast<char>(0x20));
        resp.append(static_cast<char>(0x00));
        resp.append(static_cast<char>(0x01));
        resp.append(static_cast<char>(0x00));
        resp.append(static_cast<char>(0x00));

        // Key 0x21: Sample rate max (100 MHz) -> 4 bytes big endian
        resp.append(static_cast<char>(0x21));
        resp.append(static_cast<char>(0x05));
        resp.append(static_cast<char>(0xF5));
        resp.append(static_cast<char>(0xE1));
        resp.append(static_cast<char>(0x00));

        // Key 0x40: Channels (8)
        resp.append(static_cast<char>(0x40));
        resp.append(static_cast<char>(8));

        // Key 0x00: End of metadata
        resp.append(static_cast<char>(0x00));

        injectIncomingData(resp);
        return;
    }

    // 3. SUMP Run command: 0x01
    if (cmd.contains('\x01')) {
        generateSyntheticCapture(65536);
        return;
    }

    // 4. ASCII Terminal Commands
    QString cmdStr = QString::fromUtf8(cmd).trimmed();
    if (cmdStr.isEmpty()) return;

    if (cmdStr == "help" || cmdStr == "?") {
        QString help = 
            "\r\n--- UDOO KEY Universal Terminal ---\r\n"
            "help            - Show this command list\r\n"
            "status          - Show system status and voltage rails\r\n"
            "adc             - Read analog input voltage\r\n"
            "cont            - Run continuity check\r\n"
            "i2c scan        - Scan I2C bus for 7-bit addresses\r\n"
            "spi write <hex> - Write hex byte array to SPI bus\r\n"
            "capture         - Trigger 64 KiB logic capture\r\n"
            "HiZ> ";
        injectIncomingData(help.toUtf8());
    } else if (cmdStr == "status") {
        QString status = 
            "\r\nDevice: UDOO KEY (RP2040 Dual Core M0+)\r\n"
            "Firmware: v1.0.0-PROTO8\r\n"
            "VBUS: 5.04 V | 3V3: 3.31 V | Core: 1.20 V\r\n"
            "PIO0 Engine: RUNNING (SM0 DMA armed)\r\n"
            "HiZ> ";
        injectIncomingData(status.toUtf8());
    } else if (cmdStr == "adc") {
        QString adc = "\r\nADC Ch0: 1.652 V (Raw: 2050)\r\nHiZ> ";
        injectIncomingData(adc.toUtf8());
    } else if (cmdStr == "cont") {
        QString cont = "\r\nContinuity: OPEN (Resistance > 50 Ohm)\r\nHiZ> ";
        injectIncomingData(cont.toUtf8());
    } else if (cmdStr == "i2c scan") {
        QString i2c = 
            "\r\nScanning I2C bus (SDA=GP4, SCL=GP5)...\r\n"
            "0x00: -- -- -- -- -- -- -- --\r\n"
            "0x10: -- -- -- -- -- -- -- --\r\n"
            "0x20: -- -- -- -- -- -- -- --\r\n"
            "0x30: -- -- -- -- -- -- -- --\r\n"
            "0x40: -- -- -- -- -- -- -- --\r\n"
            "0x50: 50 -- -- -- -- -- -- --\r\n"
            "0x60: -- -- -- -- -- -- -- --\r\n"
            "0x70: -- -- -- -- -- -- -- --\r\n"
            "Found 1 device(s): [0x50]\r\n"
            "HiZ> ";
        injectIncomingData(i2c.toUtf8());
    } else if (cmdStr == "capture") {
        generateSyntheticCapture(65536);
    } else {
        QString echo = QString("\r\nCommand: %1\r\nOK\r\nHiZ> ").arg(cmdStr);
        injectIncomingData(echo.toUtf8());
    }
}

} // namespace UdooKey
