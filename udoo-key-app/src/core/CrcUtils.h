#pragma once

#include <cstdint>
#include <cstddef>
#include <QByteArray>

namespace UdooKey {

class CrcUtils {
public:
    // CRC-16-CCITT (poly 0x1021, initial 0xFFFF)
    static uint16_t crc16Ccitt(const uint8_t* data, size_t length, uint16_t initial = 0xFFFF);
    static uint16_t crc16Ccitt(const QByteArray& data, uint16_t initial = 0xFFFF);

    // CRC-32 (IEEE 802.3, poly 0xEDB88320)
    static uint32_t crc32(const uint8_t* data, size_t length, uint32_t initial = 0xFFFFFFFF);
    static uint32_t crc32(const QByteArray& data, uint32_t initial = 0xFFFFFFFF);

    // Even parity calculation (returns 1 if number of set bits is odd, 0 if even, so (bits + parity) has even count)
    static inline uint8_t evenParity(uint32_t value) {
        value ^= value >> 16;
        value ^= value >> 8;
        value ^= value >> 4;
        value &= 0xF;
        return (0x6996 >> value) & 1;
    }

    // Odd parity calculation
    static inline uint8_t oddParity(uint32_t value) {
        return evenParity(value) ^ 1;
    }

    // Simple 8-bit XOR checksum
    static uint8_t xorChecksum(const uint8_t* data, size_t length);
    static uint8_t xorChecksum(const QByteArray& data);
};

} // namespace UdooKey
