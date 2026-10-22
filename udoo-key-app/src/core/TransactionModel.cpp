#include "TransactionModel.h"

namespace UdooKey {

TransactionLogger::TransactionLogger() {
}

TransactionLogger& TransactionLogger::instance() {
    static TransactionLogger s_instance;
    return s_instance;
}

uint64_t TransactionLogger::logTransaction(TransactionResult result) {
    {
        QMutexLocker locker(&m_mutex);
        if (result.id == 0) {
            result.id = m_nextId++;
        }
        if (result.timestamp.isNull()) {
            result.timestamp = QDateTime::currentDateTime();
        }

        m_transactions.append(result);
        if (m_transactions.size() > MAX_TRANSACTIONS) {
            m_transactions.removeFirst();
        }
    }

    emit transactionLogged(result);
    return result.id;
}

QList<TransactionResult> TransactionLogger::getTransactions(const QString& subsystemFilter, int maxCount) const {
    QMutexLocker locker(&m_mutex);
    QList<TransactionResult> filtered;
    filtered.reserve(std::min(maxCount, static_cast<int>(m_transactions.size())));

    for (int i = m_transactions.size() - 1; i >= 0 && filtered.size() < maxCount; --i) {
        const auto& tx = m_transactions.at(i);
        if (subsystemFilter.isEmpty() || tx.subsystem.compare(subsystemFilter, Qt::CaseInsensitive) == 0) {
            filtered.append(tx);
        }
    }

    // Reverse to chronological order
    std::reverse(filtered.begin(), filtered.end());
    return filtered;
}

TransactionResult TransactionLogger::getTransactionById(uint64_t id) const {
    QMutexLocker locker(&m_mutex);
    for (const auto& tx : m_transactions) {
        if (tx.id == id) {
            return tx;
        }
    }
    return TransactionResult{};
}

void TransactionLogger::clear() {
    {
        QMutexLocker locker(&m_mutex);
        m_transactions.clear();
    }
    emit cleared();
}

int TransactionLogger::count() const {
    QMutexLocker locker(&m_mutex);
    return m_transactions.size();
}

} // namespace UdooKey
