#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QByteArray>
#include <QVariantMap>
#include <QList>
#include <QMutex>

namespace UdooKey {

struct TransactionResult {
    uint64_t id = 0;
    QString subsystem; // "JTAG", "SWD", "I2C", "SPI", "UART", "USB", "Logic", "Terminal"
    QDateTime timestamp;
    uint64_t durationNs = 0;
    bool success = true;
    QString summary;
    QByteArray txData;
    QByteArray rxData;
    QString errorMessage;
    QVariantMap metadata;

    double durationMs() const { return static_cast<double>(durationNs) / 1'000'000.0; }
};

class TransactionLogger : public QObject {
    Q_OBJECT

public:
    static TransactionLogger& instance();

    uint64_t logTransaction(TransactionResult result);
    QList<TransactionResult> getTransactions(const QString& subsystemFilter = QString(), int maxCount = 500) const;
    TransactionResult getTransactionById(uint64_t id) const;
    void clear();
    int count() const;

signals:
    void transactionLogged(const TransactionResult& tx);
    void cleared();

private:
    TransactionLogger();
    ~TransactionLogger() override = default;
    TransactionLogger(const TransactionLogger&) = delete;
    TransactionLogger& operator=(const TransactionLogger&) = delete;

    mutable QMutex m_mutex;
    uint64_t m_nextId = 1;
    QList<TransactionResult> m_transactions;
    static constexpr int MAX_TRANSACTIONS = 5000;
};

} // namespace UdooKey
