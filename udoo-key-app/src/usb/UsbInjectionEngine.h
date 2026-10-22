#pragma once

#include <QObject>
#include <QByteArray>
#include <memory>
#include "IUsbHost.h"
#include "TransactionModel.h"

namespace UdooKey {

class UsbInjectionEngine : public QObject {
    Q_OBJECT

public:
    explicit UsbInjectionEngine(IUsbHost* host, QObject* parent = nullptr);
    ~UsbInjectionEngine() override = default;

    void setHost(IUsbHost* host);
    IUsbHost* host() const { return m_host; }

    TransactionResult executeControlTransfer(const UsbSetupPacket& setup,
                                            const QByteArray& dataOut,
                                            int timeoutMs = 1000);

    TransactionResult executeBulkTransfer(uint8_t endpoint,
                                         const QByteArray& dataOut,
                                         bool isRead,
                                         int readLength = 64,
                                         int timeoutMs = 1000);

    TransactionResult executeInterruptTransfer(uint8_t endpoint,
                                             const QByteArray& dataOut,
                                             bool isRead,
                                             int readLength = 64,
                                             int timeoutMs = 1000);

signals:
    void transferExecuted(const TransactionResult& result);

private:
    IUsbHost* m_host = nullptr;
};

} // namespace UdooKey
