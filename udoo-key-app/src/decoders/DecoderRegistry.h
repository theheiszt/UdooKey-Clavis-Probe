#pragma once

#include <QObject>
#include <QList>
#include <memory>
#include "IDecoder.h"
#include "CaptureBuffer.h"

namespace UdooKey {

class DecoderRegistry : public QObject {
    Q_OBJECT

public:
    static DecoderRegistry& instance();

    void registerDecoder(std::shared_ptr<IDecoder> decoder);
    QList<std::shared_ptr<IDecoder>> decoders() const;
    std::shared_ptr<IDecoder> decoder(ProtocolType type) const;
    std::shared_ptr<IDecoder> decoder(const QString& id) const;

    void runDecodersAsync(const CaptureBuffer& buffer);
    void runDecodersAsync(std::shared_ptr<const CaptureBuffer> buffer);

signals:
    void decoderFinished(ProtocolType type, const QVector<DecodedPacket>& packets);
    void decoderFinishedWithEvents(const QString& protocolId, const QVector<ProtocolEvent>& events, const QVector<DecodedPacket>& packets);

private:
    DecoderRegistry();
    ~DecoderRegistry() override = default;
    DecoderRegistry(const DecoderRegistry&) = delete;
    DecoderRegistry& operator=(const DecoderRegistry&) = delete;

    QList<std::shared_ptr<IDecoder>> m_decoders;
};

} // namespace UdooKey
