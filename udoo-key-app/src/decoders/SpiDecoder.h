#pragma once

#include "IDecoder.h"

namespace UdooKey {

class SpiDecoder : public IDecoder {
public:
    SpiDecoder();
    ~SpiDecoder() override = default;

    QString name() const override { return "SPI"; }
    ProtocolType type() const override { return ProtocolType::SPI; }

    QString id() const override { return "spi"; }
    QString description() const override { return "Serial Peripheral Interface (SPI) Decoder"; }

    QList<DecoderChannelBinding> channelBindings() const override {
        return {
            {"SCK", m_sckChannel, "SPI Clock"},
            {"MOSI", m_mosiChannel, "Master Out Slave In"},
            {"MISO", m_misoChannel, "Master In Slave Out"},
            {"CS", m_csChannel, "Chip Select (Active Low)"}
        };
    }

    QList<DecoderOptionDefinition> optionDefinitions() const override {
        return {
            {"mode", "SPI Mode", 0, {0, 1, 2, 3}},
            {"bitOrder", "Bit Order", "MSB First", {"MSB First", "LSB First"}},
            {"wordSize", "Word Size", 8, {8, 16, 32}}
        };
    }

    QMap<QString, QVariant> parameters() const override;
    void setParameter(const QString& key, const QVariant& value) override;

    QVector<DecodedPacket> decode(const CaptureBuffer& buffer) override;

private:
    uint8_t m_sckChannel = 4;
    uint8_t m_mosiChannel = 5;
    uint8_t m_misoChannel = 1;
    uint8_t m_csChannel = 3;
    uint8_t m_mode = 0; // 0, 1, 2, 3
    bool m_csActiveLow = true;
};

} // namespace UdooKey
