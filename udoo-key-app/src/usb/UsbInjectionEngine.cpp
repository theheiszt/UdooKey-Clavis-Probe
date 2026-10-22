#include "UsbInjectionEngine.h"
#include <QElapsedTimer>

namespace UdooKey {

UsbInjectionEngine::UsbInjectionEngine(IUsbHost* host, QObject* parent)
    : QObject(parent)
    , m_host(host) {
}

void UsbInjectionEngine::setHost(IUsbHost* host) {
    m_host = host;
}

TransactionResult UsbInjectionEngine::executeControlTransfer(const UsbSetupPacket& setup,
                                                            const QByteArray& dataOut,
                                                            int timeoutMs) {
    TransactionResult res;
    res.subsystem = "USB";
    res.timestamp = QDateTime::currentDateTime();

    if (!m_host || !m_host->isDeviceOpen()) {
        res.success = false;
        res.errorMessage = "No active USB device opened";
        res.summary = "Control Transfer failed: Device closed";
        TransactionLogger::instance().logTransaction(res);
        emit transferExecuted(res);
        return res;
    }

    QElapsedTimer timer;
    timer.start();

    QByteArray dataIn;
    res.txData = setup.toByteArray();
    if (!dataOut.isEmpty()) {
        res.txData.append(dataOut);
    }

    bool ok = m_host->controlTransfer(setup, dataOut, dataIn, timeoutMs);
    res.durationNs = timer.nsecsElapsed();
    res.rxData = dataIn;
    res.success = ok;

    res.metadata["bmRequestType"] = QString("0x%1").arg(setup.bmRequestType, 2, 16, QChar('0'));
    res.metadata["bRequest"] = QString("0x%1 (%2)").arg(setup.bRequest, 2, 16, QChar('0')).arg(setup.requestNameString());
    res.metadata["wValue"] = QString("0x%1").arg(setup.wValue, 4, 16, QChar('0'));
    res.metadata["wIndex"] = QString("0x%1").arg(setup.wIndex, 4, 16, QChar('0'));
    res.metadata["wLength"] = setup.wLength;
    res.metadata["direction"] = setup.directionString();

    res.summary = QString("CTRL %1: %2 (Len=%3, RxBytes=%4)")
                      .arg(setup.directionString())
                      .arg(setup.requestNameString())
                      .arg(setup.wLength)
                      .arg(dataIn.size());

    if (!ok) {
        res.errorMessage = "USB Host control transfer timed out or returned STALL";
    }

    TransactionLogger::instance().logTransaction(res);
    emit transferExecuted(res);
    return res;
}

TransactionResult UsbInjectionEngine::executeBulkTransfer(uint8_t endpoint,
                                                         const QByteArray& dataOut,
                                                         bool isRead,
                                                         int readLength,
                                                         int timeoutMs) {
    TransactionResult res;
    res.subsystem = "USB";
    res.timestamp = QDateTime::currentDateTime();

    if (!m_host || !m_host->isDeviceOpen()) {
        res.success = false;
        res.errorMessage = "No active USB device opened";
        res.summary = "Bulk Transfer failed: Device closed";
        TransactionLogger::instance().logTransaction(res);
        emit transferExecuted(res);
        return res;
    }

    QElapsedTimer timer;
    timer.start();

    bool ok = false;
    QByteArray dataIn;

    if (isRead) {
        ok = m_host->bulkRead(endpoint, dataIn, readLength, timeoutMs);
        res.rxData = dataIn;
    } else {
        res.txData = dataOut;
        ok = m_host->bulkWrite(endpoint, dataOut, timeoutMs);
    }

    res.durationNs = timer.nsecsElapsed();
    res.success = ok;
    res.metadata["endpoint"] = QString("0x%1").arg(endpoint, 2, 16, QChar('0')).toUpper();
    res.metadata["type"] = "Bulk";
    res.metadata["isRead"] = isRead;

    res.summary = QString("BULK %1 [EP 0x%2]: %3 bytes")
                      .arg(isRead ? "IN" : "OUT")
                      .arg(endpoint, 2, 16, QChar('0')).toUpper()
                      .arg(isRead ? dataIn.size() : dataOut.size());

    if (!ok) {
        res.errorMessage = "Bulk transfer failed or timed out";
    }

    TransactionLogger::instance().logTransaction(res);
    emit transferExecuted(res);
    return res;
}

TransactionResult UsbInjectionEngine::executeInterruptTransfer(uint8_t endpoint,
                                                             const QByteArray& dataOut,
                                                             bool isRead,
                                                             int readLength,
                                                             int timeoutMs) {
    TransactionResult res;
    res.subsystem = "USB";
    res.timestamp = QDateTime::currentDateTime();

    if (!m_host || !m_host->isDeviceOpen()) {
        res.success = false;
        res.errorMessage = "No active USB device opened";
        res.summary = "Interrupt Transfer failed: Device closed";
        TransactionLogger::instance().logTransaction(res);
        emit transferExecuted(res);
        return res;
    }

    QElapsedTimer timer;
    timer.start();

    bool ok = false;
    QByteArray dataIn;

    if (isRead) {
        ok = m_host->interruptRead(endpoint, dataIn, readLength, timeoutMs);
        res.rxData = dataIn;
    } else {
        res.txData = dataOut;
        ok = m_host->interruptWrite(endpoint, dataOut, timeoutMs);
    }

    res.durationNs = timer.nsecsElapsed();
    res.success = ok;
    res.metadata["endpoint"] = QString("0x%1").arg(endpoint, 2, 16, QChar('0')).toUpper();
    res.metadata["type"] = "Interrupt";
    res.metadata["isRead"] = isRead;

    res.summary = QString("INTR %1 [EP 0x%2]: %3 bytes")
                      .arg(isRead ? "IN" : "OUT")
                      .arg(endpoint, 2, 16, QChar('0')).toUpper()
                      .arg(isRead ? dataIn.size() : dataOut.size());

    if (!ok) {
        res.errorMessage = "Interrupt transfer failed or timed out";
    }

    TransactionLogger::instance().logTransaction(res);
    emit transferExecuted(res);
    return res;
}

} // namespace UdooKey
