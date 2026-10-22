#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdint>

namespace UdooKey {

enum class NetworkConnectionState {
    Disconnected,
    Connecting,
    HttpProbe,
    WebSocketConnect,
    Connected,
    ApNotReachable,
    Error
};

QString networkStateToString(NetworkConnectionState state);

struct Esp32StatusInfo {
    QString device;
    QString subsystem;
    QString status;
    QString wifiMode;
    QString wifiState;
    QString ssid;
    QString ip;
    uint8_t channel{1};
    uint8_t clientCount{0};
    uint32_t uptimeMs{0};
    uint32_t freeHeap{0};
    bool reachable{false};
};

class Esp32DiscoveryService : public QObject {
    Q_OBJECT

public:
    explicit Esp32DiscoveryService(QObject* parent = nullptr);
    ~Esp32DiscoveryService() override = default;

    static Esp32DiscoveryService& instance();

    void startDiscovery(const QString& host = "192.168.4.1", uint16_t port = 80);
    void stopDiscovery();
    void probeNow();

    NetworkConnectionState state() const { return m_state; }
    const Esp32StatusInfo& lastStatus() const { return m_status; }
    bool isConnected() const { return m_state == NetworkConnectionState::Connected; }

signals:
    void stateChanged(NetworkConnectionState newState);
    void esp32Discovered(const Esp32StatusInfo& info);
    void discoveryFailed(const QString& message);
    void statusUpdated(const Esp32StatusInfo& info);

private slots:
    void onProbeTimeout();
    void onReplyFinished(QNetworkReply* reply);

private:
    void setState(NetworkConnectionState newState);
    void scheduleRetry();

    QNetworkAccessManager m_netManager;
    QTimer m_probeTimer;
    NetworkConnectionState m_state{NetworkConnectionState::Disconnected};
    Esp32StatusInfo m_status;

    QString m_host{"192.168.4.1"};
    uint16_t m_port{80};
    uint32_t m_retryDelayMs{2000};
    uint8_t  m_retryAttempts{0};
    static constexpr uint8_t MAX_BACKOFF_SECONDS = 10;
};

} // namespace UdooKey
