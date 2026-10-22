#include "StreamDemux.h"
#include "CrcUtils.h"
#include "instrument_protocol.h"
#include <QByteArray>
#include <QString>
#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>

bool runStreamDemuxTests() {
    using namespace UdooKey;
    StreamDemux demux;

    bool binReceived = false;
    QByteArray binPayload;
    QObject::connect(&demux, &StreamDemux::binary0x5AFrameReceived, [&](const QByteArray& payload) {
        binReceived = true;
        binPayload = payload;
    });

    bool textReceived = false;
    QString termText;
    QObject::connect(&demux, &StreamDemux::terminalTextReceived, [&](const QString& text) {
        textReceived = true;
        termText += text;
    });

    bool pkt0xAA55Received = false;
    InstrumentPacketHeader receivedHdr{};
    QByteArray received0xAA55Payload;
    QObject::connect(&demux, &StreamDemux::instrumentPacketReceived, 
        [&](const InstrumentPacketHeader& hdr, const QByteArray& payload) {
            pkt0xAA55Received = true;
            receivedHdr = hdr;
            received0xAA55Payload = payload;
        });

    // 1. Feed pure terminal text with 'Z' characters (should not trigger false 0x5A)
    demux.processIncomingData("Hello Zone Zigzag!\r\n");
    if (!textReceived || binReceived || !termText.contains("Zone")) {
        std::cerr << "StreamDemux FAIL: terminal text test failed" << std::endl;
        return false;
    }

    // Reset
    textReceived = false;
    termText.clear();

    // 2. Feed 0x5A Binary packet: [0x5A] [0x00] [0x00] [0x04] [0xAA 0xBB 0xCC 0xDD]
    QByteArray binPkt;
    binPkt.append(static_cast<char>(0x5A));
    binPkt.append(static_cast<char>(0x00));
    binPkt.append(static_cast<char>(0x00));
    binPkt.append(static_cast<char>(0x04)); // length = 4
    binPkt.append("\xAA\xBB\xCC\xDD", 4);

    demux.processIncomingData(binPkt);
    if (!binReceived || binPayload.size() != 4 ||
        static_cast<uint8_t>(binPayload.at(0)) != 0xAA ||
        static_cast<uint8_t>(binPayload.at(3)) != 0xDD) {
        std::cerr << "StreamDemux FAIL: binPkt 0x5A failed" << std::endl;
        return false;
    }

    // 3. Feed SUMP 1ALS packet
    bool sumpReceived = false;
    QObject::connect(&demux, &StreamDemux::sumpDataReceived, [&](const QByteArray&) {
        sumpReceived = true;
    });

    demux.processIncomingData("Prompt> 1ALS HiZ>");
    if (!sumpReceived) {
        std::cerr << "StreamDemux FAIL: sumpReceived is false" << std::endl;
        return false;
    }

    // Reset for 0xAA55 tests
    demux.reset();
    textReceived = false;
    termText.clear();
    pkt0xAA55Received = false;

    // 4. Valid 0xAA55 InstrumentProtocol packet
    QByteArray validPayload = "STATUS_REPORT_OK";
    InstrumentPacketHeader validHdr{};
    validHdr.magic = INSTRUMENT_PROTO_MAGIC;
    validHdr.version = INSTRUMENT_PROTO_VERSION;
    validHdr.type = PKT_TYPE_STATUS_RESP;
    validHdr.sequence = 42;
    validHdr.payload_len = static_cast<uint16_t>(validPayload.size());
    validHdr.crc16 = CrcUtils::crc16Ccitt(
        reinterpret_cast<const uint8_t*>(validPayload.constData()), validPayload.size());

    QByteArray valid0xAA55;
    valid0xAA55.append(reinterpret_cast<const char*>(&validHdr), sizeof(validHdr));
    valid0xAA55.append(validPayload);

    demux.processIncomingData(valid0xAA55);
    if (!pkt0xAA55Received || receivedHdr.sequence != 42 || received0xAA55Payload != validPayload) {
        std::cerr << "StreamDemux FAIL: valid 0xAA55 packet test failed" << std::endl;
        return false;
    }

    // 5. Truncated 0xAA55 Packet Stream (split over multiple writes)
    demux.reset();
    pkt0xAA55Received = false;
    received0xAA55Payload.clear();

    validHdr.sequence = 99;
    valid0xAA55.clear();
    valid0xAA55.append(reinterpret_cast<const char*>(&validHdr), sizeof(validHdr));
    valid0xAA55.append(validPayload);

    // Ingest first 6 bytes of 10-byte header
    demux.processIncomingData(valid0xAA55.left(6));
    if (pkt0xAA55Received) {
        std::cerr << "StreamDemux FAIL: premature packet trigger on incomplete header" << std::endl;
        return false;
    }

    // Ingest remaining 4 bytes of header + 5 bytes of payload
    demux.processIncomingData(valid0xAA55.mid(6, 9));
    if (pkt0xAA55Received) {
        std::cerr << "StreamDemux FAIL: premature packet trigger on incomplete payload" << std::endl;
        return false;
    }
    if (demux.currentState() != DemuxState::CollectingInstrument0xAA55) {
        std::cerr << "StreamDemux FAIL: expected CollectingInstrument0xAA55 state" << std::endl;
        return false;
    }

    // Ingest rest of payload
    demux.processIncomingData(valid0xAA55.mid(15));
    if (!pkt0xAA55Received || receivedHdr.sequence != 99 || received0xAA55Payload != validPayload) {
        std::cerr << "StreamDemux FAIL: fragmented packet reassembly failed" << std::endl;
        return false;
    }
    if (demux.currentState() != DemuxState::Idle) {
        std::cerr << "StreamDemux FAIL: expected Idle state after packet finish" << std::endl;
        return false;
    }

    // 6. Malformed Header: Unsupported Version (e.g. 0xFF)
    demux.reset();
    pkt0xAA55Received = false;
    InstrumentPacketHeader badVerHdr = validHdr;
    badVerHdr.version = 0xFF; // Invalid version
    QByteArray badVerPkt;
    badVerPkt.append(reinterpret_cast<const char*>(&badVerHdr), sizeof(badVerHdr));
    badVerPkt.append(validPayload);

    demux.processIncomingData(badVerPkt);
    if (pkt0xAA55Received) {
        std::cerr << "StreamDemux FAIL: bad version packet should not be emitted" << std::endl;
        return false;
    }

    // 7. Oversized Payload Length Protection (> 2048 bytes)
    demux.reset();
    pkt0xAA55Received = false;
    InstrumentPacketHeader hugeHdr = validHdr;
    hugeHdr.payload_len = 65000; // Oversized length beyond maximum buffer
    QByteArray hugePkt;
    hugePkt.append(reinterpret_cast<const char*>(&hugeHdr), sizeof(hugeHdr));

    demux.processIncomingData(hugePkt);
    if (pkt0xAA55Received || demux.currentState() == DemuxState::CollectingInstrument0xAA55) {
        std::cerr << "StreamDemux FAIL: oversized payload length was not rejected" << std::endl;
        return false;
    }

    // 8. Interleaved Mixed Streams: Terminal Text + 0xAA55 Packet + Terminal Text
    demux.reset();
    textReceived = false;
    termText.clear();
    pkt0xAA55Received = false;

    QByteArray interleaved;
    interleaved.append("UDOO_INIT_PROMPT> ");
    interleaved.append(reinterpret_cast<const char*>(&validHdr), sizeof(validHdr));
    interleaved.append(validPayload);
    interleaved.append("\r\nUDOO_READY>\r\n");

    demux.processIncomingData(interleaved);
    if (!pkt0xAA55Received || !termText.contains("UDOO_INIT_PROMPT") || !termText.contains("UDOO_READY")) {
        std::cerr << "StreamDemux FAIL: interleaved mixed stream failed. Text: " 
                  << termText.toStdString() << std::endl;
        return false;
    }

    // 9. High-Entropy Noise & Fuzz Injection (64 KiB pseudo-random data)
    demux.reset();
    QByteArray noise;
    noise.resize(65536);
    uint32_t seed = 0x12345678;
    for (int i = 0; i < noise.size(); ++i) {
        seed = seed * 1664525 + 1013904223;
        noise[i] = static_cast<char>((seed >> 16) & 0xFF);
    }

    demux.processIncomingData(noise);
    // Demux must not crash, buffer must remain bounded
    demux.reset();
    if (demux.currentState() != DemuxState::Idle) {
        std::cerr << "StreamDemux FAIL: state not Idle after noise reset" << std::endl;
        return false;
    }

    std::cout << "  - All StreamDemux protocol robustness tests (valid, fragmented, bad version, oversized, interleaved, 64KB noise) PASSED." << std::endl;
    return true;
}
