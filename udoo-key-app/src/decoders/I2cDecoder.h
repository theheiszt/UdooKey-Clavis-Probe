#pragma once

#include "IDecoder.h"

namespace UdooKey {

class I2cDecoder : public IDecoder {
public:
    I2cDecoder();
    ~I2cDecoder() override = default;

    QString name() const override { return "I2C"; }
    ProtocolType type() const override { return ProtocolType::I2C; }

    QString id() const override { return "i2c"; }
    QString description() const override { return "Inter-Integrated Circuit (I2C) Bus Decoder"; }

    QList<DecoderChannelBinding> channelBindings() const override {
        return {
            {"SCL", m_sclChannel, "I2C Serial Clock"},
            {"SDA", m_sdaChannel, "I2C Serial Data"}
        };
    }

    QList<DecoderOptionDefinition> optionDefinitions() const override {
        return {
            {"addressFormat", "Address Format", "7-bit", {"7-bit", "10-bit"}}
        };
    }

    QMap<QString, QVariant> parameters() const override;
    void setParameter(const QString& key, const QVariant& value) override;

    QVector<DecodedPacket> decode(const CaptureBuffer& buffer) override;

private:
    uint8_t m_sclChannel = 6;
    uint8_t m_sdaChannel = 7;
};

} // namespace UdooKey
