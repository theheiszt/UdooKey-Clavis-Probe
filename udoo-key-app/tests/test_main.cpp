#include <QCoreApplication>
#include <iostream>

// Declarations of individual test suites
bool runRingBufferTests();
bool runStreamDemuxTests();
bool runBinaryLogicTests();
bool runSumpClientTests();
bool runDecoderTests();
bool runExporterTests();
bool runJtagSwdTests();
bool runArchitectureAndUsbTests();
bool runSelfTestTests();
bool runGpioClientTests();
bool runModeClientTests();
bool runBusToolsClientTests();
bool runSyntaxClientTests();
bool runWirelessClientTests();
bool runPacketPcapTests();
bool runEsp32DiscoveryTests();

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "========================================" << std::endl;
    std::cout << "  UDOO KEY C++ Test Suite (CTest)       " << std::endl;
    std::cout << "========================================" << std::endl;

    int passed = 0;
    int total = 16;

    if (runRingBufferTests()) {
        std::cout << "[PASS] RingBuffer Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] RingBuffer Tests" << std::endl;
    }

    if (runStreamDemuxTests()) {
        std::cout << "[PASS] StreamDemux Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] StreamDemux Tests" << std::endl;
    }

    if (runBinaryLogicTests()) {
        std::cout << "[PASS] BinaryLogic Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] BinaryLogic Tests" << std::endl;
    }

    if (runSumpClientTests()) {
        std::cout << "[PASS] SumpClient Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] SumpClient Tests" << std::endl;
    }

    if (runDecoderTests()) {
        std::cout << "[PASS] Decoder Tests (UART / I2C / SPI)" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] Decoder Tests" << std::endl;
    }

    if (runExporterTests()) {
        std::cout << "[PASS] Exporter Tests (VCD / CSV / BIN)" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] Exporter Tests" << std::endl;
    }

    if (runJtagSwdTests()) {
        std::cout << "[PASS] JTAG & SWD Subsystem Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] JTAG & SWD Subsystem Tests" << std::endl;
    }

    if (runArchitectureAndUsbTests()) {
        std::cout << "[PASS] Architecture, Capabilities, USB & Persistence Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] Architecture, Capabilities, USB & Persistence Tests" << std::endl;
    }

    if (runSelfTestTests()) {
        std::cout << "[PASS] Hardware Diagnostic & Self-Test Suite Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] Hardware Diagnostic & Self-Test Suite Tests" << std::endl;
    }

    if (runGpioClientTests()) {
        std::cout << "[PASS] Bus Pirate GPIO Service Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] Bus Pirate GPIO Service Tests" << std::endl;
    }

    if (runModeClientTests()) {
        std::cout << "[PASS] Bus Pirate Mode Multiplexer & Invariants Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] Bus Pirate Mode Multiplexer & Invariants Tests" << std::endl;
    }

    if (runBusToolsClientTests()) {
        std::cout << "[PASS] Bus Pirate Hardware Protocol Engines Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] Bus Pirate Hardware Protocol Engines Tests" << std::endl;
    }

    if (runSyntaxClientTests()) {
        std::cout << "[PASS] Bus Pirate Interactive Syntax & Command Execution Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] Bus Pirate Interactive Syntax & Command Execution Tests" << std::endl;
    }

    if (runWirelessClientTests()) {
        std::cout << "[PASS] Bus Pirate Wireless & Coprocessor (IR & nRF24L01) Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] Bus Pirate Wireless & Coprocessor (IR & nRF24L01) Tests" << std::endl;
    }

    if (runPacketPcapTests()) {
        std::cout << "[PASS] Bus Pirate Packet & PCAP Analysis Protocol Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] Bus Pirate Packet & PCAP Analysis Protocol Tests" << std::endl;
    }

    if (runEsp32DiscoveryTests()) {
        std::cout << "[PASS] ESP32 Discovery & HTTP Integration Tests" << std::endl;
        passed++;
    } else {
        std::cerr << "[FAIL] ESP32 Discovery & HTTP Integration Tests" << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << passed << " / " << total << " test suites passed." << std::endl;
    std::cout << "========================================" << std::endl;

    return (passed == total) ? 0 : 1;
}
