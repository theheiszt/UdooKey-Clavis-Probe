#pragma once

#include <QString>
#include <QVector>
#include <QList>
#include <QMap>
#include <QVariant>
#include <memory>
#include "Types.h"
#include "CaptureBuffer.h"
#include "TaskManager.h"

namespace UdooKey {

class IDecoder {
public:
    virtual ~IDecoder() = default;

    virtual QString name() const = 0;
    virtual ProtocolType type() const = 0;

    virtual QString id() const {
        return name().toLower().replace(" ", "_");
    }

    virtual QString description() const {
        return name() + " Protocol Decoder";
    }

    virtual QList<DecoderChannelBinding> channelBindings() const {
        return {};
    }

    virtual QList<DecoderOptionDefinition> optionDefinitions() const {
        return {};
    }

    virtual QMap<QString, QVariant> parameters() const = 0;
    virtual void setParameter(const QString& key, const QVariant& value) = 0;

    virtual QVector<DecodedPacket> decode(const CaptureBuffer& buffer) = 0;

    virtual QVector<ProtocolEvent> decodeEvents(const CaptureBuffer& buffer,
                                                std::shared_ptr<CancellationToken> token = nullptr) {
        Q_UNUSED(token);
        QVector<ProtocolEvent> events;
        auto packets = decode(buffer);
        for (const auto& pkt : packets) {
            ProtocolEvent ev;
            ev.sampleIndex = pkt.startSample;
            ev.timestampSec = buffer.sampleToTimeSec(pkt.startSample);
            ev.protocolId = id();
            ev.eventType = "PACKET";
            ev.formattedValue = pkt.title;
            ev.annotation = pkt.title;
            events.append(ev);

            for (const auto& tok : pkt.tokens) {
                ProtocolEvent tokEv;
                tokEv.sampleIndex = tok.startSample;
                tokEv.timestampSec = tok.startTimeSec;
                tokEv.protocolId = id();
                tokEv.eventType = "TOKEN";
                tokEv.formattedValue = tok.text;
                tokEv.isError = tok.isWarning;
                tokEv.annotation = tok.text;
                events.append(tokEv);
            }
        }
        return events;
    }
};

} // namespace UdooKey
