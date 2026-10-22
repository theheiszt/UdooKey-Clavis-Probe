#include "SumpClient.h"
#include "MockTransport.h"
#include <iostream>

bool runSumpClientTests() {
    using namespace UdooKey;
    MockTransport mock;
    mock.open();

    SumpClient client(&mock);
    QObject::connect(&mock, &ITransport::dataReady, &client, &SumpClient::onSumpDataReceived);

    bool idReceived = false;
    QString deviceId;
    QObject::connect(&client, &SumpClient::deviceIdReceived, [&](const QString& id) {
        idReceived = true;
        deviceId = id;
    });

    bool metaReceived = false;
    QMap<int, QVariant> metadata;
    QObject::connect(&client, &SumpClient::metadataReceived, [&](const QMap<int, QVariant>& meta) {
        metaReceived = true;
        metadata = meta;
    });

    // 1. Query ID (MockTransport automatically replies with "1ALS")
    client.queryId();
    if (!idReceived) return false;
    if (!deviceId.contains("1ALS")) return false;

    // 2. Query Metadata (MockTransport automatically replies with metadata block)
    client.queryMetadata();
    if (!metaReceived) return false;
    if (!metadata.contains(0x01)) return false; // Name
    if (!metadata.value(0x01).toString().contains("UDOO KEY")) return false;

    return true;
}
