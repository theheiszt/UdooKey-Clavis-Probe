#include "DecoderRegistry.h"
#include "UartDecoder.h"
#include "I2cDecoder.h"
#include "SpiDecoder.h"
#include "Logger.h"
#include "TaskManager.h"

namespace UdooKey {

DecoderRegistry::DecoderRegistry() {
    registerDecoder(std::make_shared<UartDecoder>());
    registerDecoder(std::make_shared<I2cDecoder>());
    registerDecoder(std::make_shared<SpiDecoder>());
}

DecoderRegistry& DecoderRegistry::instance() {
    static DecoderRegistry s_instance;
    return s_instance;
}

void DecoderRegistry::registerDecoder(std::shared_ptr<IDecoder> decoder) {
    if (decoder) {
        // Prevent duplicate registration
        for (const auto& existing : m_decoders) {
            if (existing->id() == decoder->id()) return;
        }
        m_decoders.append(decoder);
    }
}

QList<std::shared_ptr<IDecoder>> DecoderRegistry::decoders() const {
    return m_decoders;
}

std::shared_ptr<IDecoder> DecoderRegistry::decoder(ProtocolType type) const {
    for (const auto& dec : m_decoders) {
        if (dec->type() == type) return dec;
    }
    return nullptr;
}

std::shared_ptr<IDecoder> DecoderRegistry::decoder(const QString& id) const {
    QString target = id.toLower();
    for (const auto& dec : m_decoders) {
        if (dec->id() == target || dec->name().compare(id, Qt::CaseInsensitive) == 0) {
            return dec;
        }
    }
    return nullptr;
}

void DecoderRegistry::runDecodersAsync(const CaptureBuffer& buffer) {
    runDecodersAsync(std::make_shared<CaptureBuffer>(buffer));
}

void DecoderRegistry::runDecodersAsync(std::shared_ptr<const CaptureBuffer> buffer) {
    if (!buffer || buffer->isEmpty()) return;

    for (const auto& dec : m_decoders) {
        ProtocolType type = dec->type();
        std::shared_ptr<IDecoder> decPtr = dec;
        std::shared_ptr<const CaptureBuffer> bufPtr = buffer;

        TaskManager::instance().submitTask(
            QString("Decode %1").arg(decPtr->name()),
            QString("Decoding %1 samples").arg(bufPtr->sampleCount()),
            [this, decPtr, type, bufPtr](std::shared_ptr<CancellationToken> token, std::function<void(int, const QString&)> progress) {
                if (token && token->isCancelled()) return false;
                if (progress) progress(10, QString("Running %1 decoder...").arg(decPtr->name()));

                QVector<DecodedPacket> packets = decPtr->decode(*bufPtr);
                if (token && token->isCancelled()) return false;

                if (progress) progress(70, QString("Generating %1 protocol events...").arg(decPtr->name()));
                QVector<ProtocolEvent> events = decPtr->decodeEvents(*bufPtr, token);

                if (progress) progress(100, QString("%1 finished (%2 packets)").arg(decPtr->name()).arg(packets.size()));
                LOG_INFO(QString("Decoder %1 finished: %2 packets, %3 events")
                             .arg(decPtr->name())
                             .arg(packets.size())
                             .arg(events.size()));

                emit decoderFinished(type, packets);
                emit decoderFinishedWithEvents(decPtr->id(), events, packets);
                return true;
            }
        );
    }
}

} // namespace UdooKey
