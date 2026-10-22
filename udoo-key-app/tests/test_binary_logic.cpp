#include "BinaryLogicClient.h"
#include "MockTransport.h"
#include <iostream>

bool runBinaryLogicTests() {
    using namespace UdooKey;
    MockTransport mock;
    mock.open();

    BinaryLogicClient client(&mock);

    bool captureStarted = false;
    bool captureComplete = false;
    QByteArray receivedSamples;

    QObject::connect(&client, &BinaryLogicClient::captureStarted, [&]() {
        captureStarted = true;
    });

    QObject::connect(&client, &BinaryLogicClient::captureComplete, [&](const QByteArray& samples, uint32_t) {
        captureComplete = true;
        receivedSamples = samples;
    });

    AcquisitionConfig cfg;
    cfg.sampleRateHz = 10000000;
    cfg.sampleDepth = 1024;
    client.armCapture(cfg);

    if (!captureStarted) return false;

    // Simulate arriving 1024 samples
    QByteArray testSamples(1024, static_cast<char>(0x55));
    client.onBinaryFrameReceived(testSamples);

    if (!captureComplete) return false;
    if (receivedSamples.size() != 1024) return false;
    if (static_cast<uint8_t>(receivedSamples.at(0)) != 0x55) return false;

    return true;
}
