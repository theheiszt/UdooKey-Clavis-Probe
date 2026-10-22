#pragma once

#include "IDecoder.h"

namespace UdooKey {

class UartDecoder : public IDecoder {
public:
    UartDecoder();
    ~UartDecoder() override = default;

    QString name() const override { return "UART"; }
    ProtocolType type() const override { return ProtocolType::UART; }

    QString id() const override { return "uart"; }
    QString description() const override { return "Asynchronous Serial 8N1 Decoder"; }

    QList<DecoderChannelBinding> channelBindings() const override {
        return { {"RX", m_channel, "Serial Receive line"} };
    }

    QList<DecoderOptionDefinition> optionDefinitions() const override {
        return {
            {"baudRate", "Baud Rate", 115200, {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600}},
            {"dataBits", "Data Bits", 8, {7, 8}},
            {"parity", "Parity", "None", {"None", "Even", "Odd"}},
            {"stopBits", "Stop Bits", 1, {1, 2}}
        };
    }

    QMap<QString, QVariant> parameters() const override;
    void setParameter(const QString& key, const QVariant& value) override;

    QVector<DecodedPacket> decode(const CaptureBuffer& buffer) override;

private:
    uint8_t m_channel = 2;
    uint32_t m_baudRate = 115200;
    uint8_t m_dataBits = 8;
    QString m_parity = "None"; // None, Even, Odd
    uint8_t m_stopBits = 1;
};

} // namespace UdooKey
