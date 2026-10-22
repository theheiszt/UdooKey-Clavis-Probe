#pragma once

#include <QString>
#include <QByteArray>
#include <cstdint>
#include <vector>

namespace UdooKey {

class HexUtils {
public:
    // Format raw bytes as hex string separated by delimiter (default: space)
    static QString formatHex(const QByteArray& data, const QString& delimiter = " ", bool uppercase = true);
    static QString formatHex(const uint8_t* data, size_t length, const QString& delimiter = " ", bool uppercase = true);

    // Format single integer as hex string
    static QString formatHexByte(uint8_t byte, bool uppercase = true);
    static QString formatHexWord(uint16_t word, bool uppercase = true);
    static QString formatHexDWord(uint32_t dword, bool uppercase = true);
    static QString formatHexQWord(uint64_t qword, bool uppercase = true);

    // Parse hex string (e.g. "9F 00 AA" or "0x9F, 0x00" or "9F00AA") into byte array
    static QByteArray parseHex(const QString& hexString);

    // Format formatted hex dump with offset, hex bytes, and ASCII representation
    static QString formatHexDump(const QByteArray& data, uint64_t startAddress = 0, int bytesPerLine = 16);

    // High performance nibble conversions
    static inline char nibbleToChar(uint8_t nibble, bool uppercase = true) {
        nibble &= 0x0F;
        if (nibble < 10) return static_cast<char>('0' + nibble);
        return static_cast<char>((uppercase ? 'A' : 'a') + (nibble - 10));
    }

    static inline int charToNibble(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    }
};

} // namespace UdooKey
