#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include "Types.h"

namespace UdooKey {

class SettingsManager : public QObject {
    Q_OBJECT

public:
    static SettingsManager& instance();

    QString lastPort() const;
    void setLastPort(const QString& port);

    int lastBaudRate() const;
    void setLastBaudRate(int baud);

    AcquisitionConfig acquisitionConfig() const;
    void setAcquisitionConfig(const AcquisitionConfig& config);

    ChannelConfig channelConfig(uint8_t index) const;
    void setChannelConfig(uint8_t index, const ChannelConfig& config);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geom);

    QByteArray windowState() const;
    void setWindowState(const QByteArray& state);

private:
    SettingsManager();
    ~SettingsManager() override = default;
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    QSettings m_settings;
};

} // namespace UdooKey
