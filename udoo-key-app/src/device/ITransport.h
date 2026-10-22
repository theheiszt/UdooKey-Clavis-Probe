#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

namespace UdooKey {

class ITransport : public QObject {
    Q_OBJECT

public:
    explicit ITransport(QObject* parent = nullptr) : QObject(parent) {}
    ~ITransport() override = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual qint64 write(const QByteArray& data) = 0;
    virtual qint64 write(const char* data, qint64 maxSize) = 0;
    virtual QByteArray readAll() = 0;
    virtual qint64 bytesAvailable() const = 0;

    virtual QString portName() const = 0;
    virtual void setPortName(const QString& name) = 0;

signals:
    void dataReady(const QByteArray& data);
    void connected();
    void disconnected();
    void errorOccurred(const QString& error);
};

} // namespace UdooKey
