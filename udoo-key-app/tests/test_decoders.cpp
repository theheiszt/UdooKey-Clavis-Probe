#include "UartDecoder.h"
#include "I2cDecoder.h"
#include "SpiDecoder.h"
#include "CaptureBuffer.h"
#include <iostream>

bool runDecoderTests() {
    using namespace UdooKey;

    // 1. Test UART Decoder: Synthesize 'A' (0x41 = 0b01000001) on CH2
    // Baud = 100,000, SampleRate = 1,000,000 (10 samples per bit)
    // Idle: High (1)
    // Start bit: Low (0) for 10 samples
    // Bit 0: 1 for 10 samples
    // Bit 1: 0 for 10 samples
    // Bit 2: 0 for 10 samples
    // Bit 3: 0 for 10 samples
    // Bit 4: 0 for 10 samples
    // Bit 5: 0 for 10 samples
    // Bit 6: 1 for 10 samples
    // Bit 7: 0 for 10 samples
    // Stop bit: 1 for 10 samples
    QByteArray uartData;
    uint8_t chMask = (1 << 2);

    auto appendBit = [&](bool high, int count) {
        for (int i = 0; i < count; ++i) {
            uartData.append(static_cast<char>(high ? chMask : 0));
        }
    };

    appendBit(true, 20);  // Idle
    appendBit(false, 10); // Start
    appendBit(true, 10);  // D0 = 1
    appendBit(false, 10); // D1 = 0
    appendBit(false, 10); // D2 = 0
    appendBit(false, 10); // D3 = 0
    appendBit(false, 10); // D4 = 0
    appendBit(false, 10); // D5 = 0
    appendBit(true, 10);  // D6 = 1
    appendBit(false, 10); // D7 = 0
    appendBit(true, 20);  // Stop & idle

    CaptureBuffer uartBuf(uartData, 1000000);
    UartDecoder uartDec;
    uartDec.setParameter("channel", 2);
    uartDec.setParameter("baudRate", 100000);

    auto uartPackets = uartDec.decode(uartBuf);
    if (uartPackets.size() != 1) return false;
    if (uartPackets[0].title != "0X41") return false;

    // 2. Test I2C Decoder: START condition followed by Address 0x50 WRITE with ACK
    // SCL = CH6 (1 << 6 = 0x40), SDA = CH7 (1 << 7 = 0x80)
    QByteArray i2cData;
    auto appendI2c = [&](bool scl, bool sda, int count) {
        uint8_t byte = (scl ? 0x40 : 0) | (sda ? 0x80 : 0);
        for (int i = 0; i < count; ++i) i2cData.append(static_cast<char>(byte));
    };

    // Idle
    appendI2c(true, true, 10);
    // START: SDA falls while SCL is high
    appendI2c(true, false, 10);
    // Address byte 0x50 WRITE -> 0xA0 (1010 0000)
    uint8_t addrByte = 0xA0;
    for (int b = 7; b >= 0; --b) {
        bool bit = (addrByte & (1 << b)) != 0;
        appendI2c(false, bit, 5); // SCL low
        appendI2c(true, bit, 5);  // SCL high (sample)
    }
    // 9th bit: ACK (SDA low)
    appendI2c(false, false, 5);
    appendI2c(true, false, 5);
    // STOP: SDA rises while SCL is high
    appendI2c(false, false, 5);
    appendI2c(true, false, 5);
    appendI2c(true, true, 10);

    CaptureBuffer i2cBuf(i2cData, 1000000);
    I2cDecoder i2cDec;
    i2cDec.setParameter("sclChannel", 6);
    i2cDec.setParameter("sdaChannel", 7);

    auto i2cPackets = i2cDec.decode(i2cBuf);
    if (i2cPackets.size() < 3) return false; // START, Addr, STOP
    if (i2cPackets[0].title != "START") return false;
    if (!i2cPackets[1].title.contains("0x50", Qt::CaseInsensitive)) return false;

    return true;
}
