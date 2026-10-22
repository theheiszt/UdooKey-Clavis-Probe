#pragma once

#include <QObject>
#include <QByteArray>
#include <QMap>
#include <QVariant>
#include "ITransport.h"
#include "Types.h"

namespace UdooKey {

class SumpClient : public QObject {
    Q_OBJECT

public:
    explicit SumpClient(ITransport* transport, QObject* parent = nullptr);
    ~SumpClient() override = default;

    void setTransport(ITransport* transport);

    void reset();
    void queryId();
    void queryMetadata();
    void setDivider(uint32_t divider);
    void setReadAndDelayCount(uint16_t readCount, uint16_t delayCount);
    void setFlags(uint16_t flags);
    void armRun();

    void onSumpDataReceived(const QByteArray& data);

signals:
    void deviceIdReceived(const QString& id);
    void metadataReceived(const QMap<int, QVariant>& metadata);
    void captureReceived(const QByteArray& samples);

private:
    void sendShortCommand(uint8_t cmd);
    void sendLongCommand(uint8_t cmd, uint32_t data);

    ITransport* m_transport;
    bool m_expectingId = false;
    bool m_expectingMeta = false;
};

} // namespace UdooKey
