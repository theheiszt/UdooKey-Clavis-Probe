#include "HexUtils.h"
#include <QStringList>
#include <QTextStream>
#include <cctype>

namespace UdooKey {

QString HexUtils::formatHex(const QByteArray& data, const QString& delimiter, bool uppercase) {
    return formatHex(reinterpret_cast<const uint8_t*>(data.constData()), static_cast<size_t>(data.size()), delimiter, uppercase);
}

QString HexUtils::formatHex(const uint8_t* data, size_t length, const QString& delimiter, bool uppercase) {
    if (!data || length == 0) return QString();

    QString result;
    result.reserve(static_cast<int>(length * (2 + delimiter.length())));

    for (size_t i = 0; i < length; ++i) {
        if (i > 0 && !delimiter.isEmpty()) {
            result.append(delimiter);
        }
        uint8_t byte = data[i];
        result.append(QChar(nibbleToChar((byte >> 4) & 0x0F, uppercase)));
        result.append(QChar(nibbleToChar(byte & 0x0F, uppercase)));
    }
    return result;
}

QString HexUtils::formatHexByte(uint8_t byte, bool uppercase) {
    QString res;
    res.reserve(2);
    res.append(QChar(nibbleToChar((byte >> 4) & 0x0F, uppercase)));
    res.append(QChar(nibbleToChar(byte & 0x0F, uppercase)));
    return res;
}

QString HexUtils::formatHexWord(uint16_t word, bool uppercase) {
    QString res;
    res.reserve(4);
    res.append(QChar(nibbleToChar((word >> 12) & 0x0F, uppercase)));
    res.append(QChar(nibbleToChar((word >> 8) & 0x0F, uppercase)));
    res.append(QChar(nibbleToChar((word >> 4) & 0x0F, uppercase)));
    res.append(QChar(nibbleToChar(word & 0x0F, uppercase)));
    return res;
}

QString HexUtils::formatHexDWord(uint32_t dword, bool uppercase) {
    QString res;
    res.reserve(8);
    for (int i = 28; i >= 0; i -= 4) {
        res.append(QChar(nibbleToChar((dword >> i) & 0x0F, uppercase)));
    }
    return res;
}

QString HexUtils::formatHexQWord(uint64_t qword, bool uppercase) {
    QString res;
    res.reserve(16);
    for (int i = 60; i >= 0; i -= 4) {
        res.append(QChar(nibbleToChar((qword >> i) & 0x0F, uppercase)));
    }
    return res;
}

QByteArray HexUtils::parseHex(const QString& hexString) {
    QByteArray result;
    result.reserve(hexString.length() / 2);

    int highNibble = -1;

    for (int i = 0; i < hexString.length(); ++i) {
        QChar qc = hexString.at(i);
        char c = qc.toLatin1();

        // Handle '0x' or '0X' prefix
        if (c == '0' && i + 1 < hexString.length() && (hexString.at(i + 1) == 'x' || hexString.at(i + 1) == 'X')) {
            i++; // skip 'x'
            continue;
        }

        int nibble = charToNibble(c);
        if (nibble < 0) {
            // Ignore separators: space, comma, semicolon, tab, newline
            continue;
        }

        if (highNibble < 0) {
            highNibble = nibble;
        } else {
            uint8_t byte = static_cast<uint8_t>((highNibble << 4) | nibble);
            result.append(static_cast<char>(byte));
            highNibble = -1;
        }
    }

    if (highNibble >= 0) {
        // Trailing single nibble treated as 0x0N
        result.append(static_cast<char>(highNibble));
    }

    return result;
}

QString HexUtils::formatHexDump(const QByteArray& data, uint64_t startAddress, int bytesPerLine) {
    if (data.isEmpty() || bytesPerLine <= 0) return QString();

    QString out;
    QTextStream stream(&out);

    int totalBytes = data.size();
    for (int rowStart = 0; rowStart < totalBytes; rowStart += bytesPerLine) {
        // Address offset: 8 hex digits
        uint64_t lineAddr = startAddress + rowStart;
        stream << QString("%1: ").arg(lineAddr, 8, 16, QChar('0')).toUpper();

        // Hex bytes
        int lineLen = std::min(bytesPerLine, totalBytes - rowStart);
        for (int i = 0; i < bytesPerLine; ++i) {
            if (i < lineLen) {
                uint8_t b = static_cast<uint8_t>(data.at(rowStart + i));
                stream << formatHexByte(b) << " ";
            } else {
                stream << "   ";
            }
            if (i == 7) stream << " "; // Center divider
        }

        stream << " |";

        // ASCII representation
        for (int i = 0; i < lineLen; ++i) {
            uint8_t b = static_cast<uint8_t>(data.at(rowStart + i));
            char c = (b >= 32 && b <= 126) ? static_cast<char>(b) : '.';
            stream << c;
        }
        stream << "|\n";
    }

    return out;
}

} // namespace UdooKey
