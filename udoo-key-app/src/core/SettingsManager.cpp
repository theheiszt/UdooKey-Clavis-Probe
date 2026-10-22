#include "SettingsManager.h"

namespace UdooKey {

SettingsManager::SettingsManager() 
    : m_settings("UdooKeyProject", "UdooKeyCompanion") {
}

SettingsManager& SettingsManager::instance() {
    static SettingsManager s_instance;
    return s_instance;
}

QString SettingsManager::lastPort() const {
    return m_settings.value("device/lastPort", "").toString();
}

void SettingsManager::setLastPort(const QString& port) {
    m_settings.setValue("device/lastPort", port);
}

int SettingsManager::lastBaudRate() const {
    return m_settings.value("device/lastBaudRate", 921600).toInt();
}

void SettingsManager::setLastBaudRate(int baud) {
    m_settings.setValue("device/lastBaudRate", baud);
}

AcquisitionConfig SettingsManager::acquisitionConfig() const {
    AcquisitionConfig cfg;
    cfg.sampleRateHz = m_settings.value("acquisition/sampleRateHz", 10'000'000).toUInt();
    cfg.sampleDepth = m_settings.value("acquisition/sampleDepth", 65536).toUInt();
    cfg.triggerChannel = static_cast<uint8_t>(m_settings.value("acquisition/triggerChannel", 0).toUInt());
    cfg.triggerMode = static_cast<TriggerMode>(m_settings.value("acquisition/triggerMode", 0).toUInt());
    cfg.preTriggerRatio = m_settings.value("acquisition/preTriggerRatio", 0.10).toDouble();
    return cfg;
}

void SettingsManager::setAcquisitionConfig(const AcquisitionConfig& config) {
    m_settings.setValue("acquisition/sampleRateHz", config.sampleRateHz);
    m_settings.setValue("acquisition/sampleDepth", config.sampleDepth);
    m_settings.setValue("acquisition/triggerChannel", config.triggerChannel);
    m_settings.setValue("acquisition/triggerMode", static_cast<uint8_t>(config.triggerMode));
    m_settings.setValue("acquisition/preTriggerRatio", config.preTriggerRatio);
}

ChannelConfig SettingsManager::channelConfig(uint8_t index) const {
    ChannelConfig ch;
    ch.index = index;
    ch.name = m_settings.value(QString("channel/%1/name").arg(index), QString("CH%1").arg(index)).toString();
    ch.color = m_settings.value(QString("channel/%1/color").arg(index), 0x00FFCC).toUInt();
    ch.enabled = m_settings.value(QString("channel/%1/enabled").arg(index), true).toBool();
    ch.triggerMode = static_cast<TriggerMode>(m_settings.value(QString("channel/%1/triggerMode").arg(index), 0).toUInt());
    return ch;
}

void SettingsManager::setChannelConfig(uint8_t index, const ChannelConfig& config) {
    m_settings.setValue(QString("channel/%1/name").arg(index), config.name);
    m_settings.setValue(QString("channel/%1/color").arg(index), config.color);
    m_settings.setValue(QString("channel/%1/enabled").arg(index), config.enabled);
    m_settings.setValue(QString("channel/%1/triggerMode").arg(index), static_cast<uint8_t>(config.triggerMode));
}

QByteArray SettingsManager::windowGeometry() const {
    return m_settings.value("ui/geometry").toByteArray();
}

void SettingsManager::setWindowGeometry(const QByteArray& geom) {
    m_settings.setValue("ui/geometry", geom);
}

QByteArray SettingsManager::windowState() const {
    return m_settings.value("ui/state").toByteArray();
}

void SettingsManager::setWindowState(const QByteArray& state) {
    m_settings.setValue("ui/state", state);
}

} // namespace UdooKey
