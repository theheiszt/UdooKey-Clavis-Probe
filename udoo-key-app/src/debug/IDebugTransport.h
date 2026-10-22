#pragma once

#include <QObject>
#include <QString>
#include "DebugProtocol.h"

namespace udoo::debug {

class IDebugTransport : public QObject {
    Q_OBJECT

public:
    explicit IDebugTransport(QObject* parent = nullptr) : QObject(parent) {}
    ~IDebugTransport() override = default;

    virtual bool isConnected() const = 0;
    virtual bool open() = 0;
    virtual void close() = 0;

    /// Send a request packet asynchronously
    virtual bool sendPacket(const DebugPacket& packet) = 0;

    /// Send a request packet and wait synchronously for the matching response
    virtual DebugPacket sendTransaction(const DebugPacket& request, int timeoutMs = 1000) = 0;

signals:
    void packetReceived(const DebugPacket& packet);
    void connectionChanged(bool connected);
    void errorOccurred(const QString& error);
};

} // namespace udoo::debug
