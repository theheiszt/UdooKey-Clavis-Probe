#include "CrcUtils.h"

namespace UdooKey {

uint16_t CrcUtils::crc16Ccitt(const uint8_t* data, size_t length, uint16_t initial) {
    uint16_t crc = initial;
    if (!data) return crc;

    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint16_t CrcUtils::crc16Ccitt(const QByteArray& data, uint16_t initial) {
    return crc16Ccitt(reinterpret_cast<const uint8_t*>(data.constData()), static_cast<size_t>(data.size()), initial);
}

static void initCrc32Table(uint32_t table[256]) {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }
}

uint32_t CrcUtils::crc32(const uint8_t* data, size_t length, uint32_t initial) {
    static uint32_t table[256];
    static bool tableInitialized = false;
    if (!tableInitialized) {
        initCrc32Table(table);
        tableInitialized = true;
    }

    uint32_t crc = initial;
    if (!data) return crc ^ 0xFFFFFFFF;

    for (size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

uint32_t CrcUtils::crc32(const QByteArray& data, uint32_t initial) {
    return crc32(reinterpret_cast<const uint8_t*>(data.constData()), static_cast<size_t>(data.size()), initial);
}

uint8_t CrcUtils::xorChecksum(const uint8_t* data, size_t length) {
    uint8_t chk = 0;
    if (!data) return 0;
    for (size_t i = 0; i < length; ++i) {
        chk ^= data[i];
    }
    return chk;
}

uint8_t CrcUtils::xorChecksum(const QByteArray& data) {
    return xorChecksum(reinterpret_cast<const uint8_t*>(data.constData()), static_cast<size_t>(data.size()));
}

} // namespace UdooKey
