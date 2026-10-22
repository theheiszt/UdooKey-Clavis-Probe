#include "Esp32DiscoveryService.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>
#include <algorithm>

namespace UdooKey {

QString networkStateToString(NetworkConnectionState state) {
    switch (state) {
        case NetworkConnectionState::Disconnected:     return "DISCONNECTED";
        case NetworkConnectionState::Connecting:       return "CONNECTING";
        case NetworkConnectionState::HttpProbe:        return "HTTP_PROBE";
        case NetworkConnectionState::WebSocketConnect: return "WEBSOCKET_CONNECT";
        case NetworkConnectionState::Connected:        return "CONNECTED";
        case NetworkConnectionState::ApNotReachable:   return "ESP32 AP not reachable";
        case NetworkConnectionState::Error:            return "ERROR";
        default:                                       return "UNKNOWN";
    }
}

Esp32DiscoveryService::Esp32DiscoveryService(QObject* parent)
    : QObject(parent) {
    connect(&m_netManager, &QNetworkAccessManager::finished, this, &Esp32DiscoveryService::onReplyFinished);
    connect(&m_probeTimer, &QTimer::timeout, this, &Esp32DiscoveryService::onProbeTimeout);
    m_probeTimer.setSingleShot(true);
}

Esp32DiscoveryService& Esp32DiscoveryService::instance() {
    static Esp32DiscoveryService s_instance;
    return s_instance;
}

void Esp32DiscoveryService::startDiscovery(const QString& host, uint16_t port) {
    m_host = host;
    m_port = port;
    m_retryDelayMs = 2000;
    m_retryAttempts = 0;
    setState(NetworkConnectionState::Connecting);
    probeNow();
}

void Esp32DiscoveryService::stopDiscovery() {
    m_probeTimer.stop();
    setState(NetworkConnectionState::Disconnected);
}

void Esp32DiscoveryService::probeNow() {
    setState(NetworkConnectionState::HttpProbe);

    QUrl url(QString("http://%1:%2/api/status").arg(m_host).arg(m_port));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "UDOO-Key-Companion/1.2.0");
    request.setTransferTimeout(2500); // 2.5 second timeout

    m_netManager.get(request);
}

void Esp32DiscoveryService::onReplyFinished(QNetworkReply* reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        m_status.reachable = false;
        setState(NetworkConnectionState::ApNotReachable);
        emit discoveryFailed(QString("ESP32 AP not reachable (%1)").arg(reply->errorString()));
        scheduleRetry();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        m_status.reachable = false;
        setState(NetworkConnectionState::ApNotReachable);
        emit discoveryFailed("Invalid JSON response from ESP32");
        scheduleRetry();
        return;
    }

    QJsonObject root = doc.object();
    m_status.reachable = true;
    m_status.device = root.value("device").toString();
    m_status.subsystem = root.value("subsystem").toString();
    m_status.status = root.value("status").toString();
    m_status.uptimeMs = root.value("uptime_ms").toInt();
    m_status.freeHeap = root.value("free_heap").toInt();

    if (root.contains("wifi") && root.value("wifi").isObject()) {
        QJsonObject wf = root.value("wifi").toObject();
        m_status.wifiMode = wf.value("mode").toString();
        m_status.wifiState = wf.value("state").toString();
        m_status.ssid = wf.value("ssid").toString();
        m_status.ip = wf.value("ip").toString();
        m_status.channel = static_cast<uint8_t>(wf.value("channel").toInt(1));
        m_status.clientCount = static_cast<uint8_t>(wf.value("clients").toInt(0));
    }

    m_retryAttempts = 0;
    m_retryDelayMs = 2000;
    setState(NetworkConnectionState::Connected);
    emit esp32Discovered(m_status);
    emit statusUpdated(m_status);

    // Schedule next polling probe
    m_probeTimer.start(5000);
}

void Esp32DiscoveryService::onProbeTimeout() {
    probeNow();
}

void Esp32DiscoveryService::setState(NetworkConnectionState newState) {
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(m_state);
    }
}

void Esp32DiscoveryService::scheduleRetry() {
    m_retryAttempts++;
    // Exponential bounded backoff: 2s, 4s, 8s, up to 10s max
    m_retryDelayMs = std::min<uint32_t>(m_retryDelayMs * 2, MAX_BACKOFF_SECONDS * 1000);
    m_probeTimer.start(m_retryDelayMs);
}

} // namespace UdooKey
