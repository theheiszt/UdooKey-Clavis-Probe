#include <cassert>
#include <iostream>
#include <vector>
#include "JtagTypes.h"
#include "JtagTap.h"
#include "SwdTypes.h"
#include "DebugProtocol.h"
#include "MockDebugTransport.h"
#include "JtagClient.h"
#include "SwdClient.h"

using namespace udoo::debug;

void testJtagTapTransitions() {
    std::cout << "[TEST] JtagTap Transitions & Navigation..." << std::endl;

    JtagTap tap;
    assert(tap.currentState() == JtagTapState::TestLogicReset);

    // Clock TMS=0: TestLogicReset -> RunTestIdle
    tap.clock(false);
    assert(tap.currentState() == JtagTapState::RunTestIdle);

    // Clock TMS=1: RunTestIdle -> SelectDrScan
    tap.clock(true);
    assert(tap.currentState() == JtagTapState::SelectDrScan);

    // Clock TMS=0: SelectDrScan -> CaptureDr
    tap.clock(false);
    assert(tap.currentState() == JtagTapState::CaptureDr);

    // Clock TMS=0: CaptureDr -> ShiftDr
    tap.clock(false);
    assert(tap.currentState() == JtagTapState::ShiftDr);

    // Verify resetSequence() reaches TestLogicReset from ALL 16 states
    for (int stateVal = 0; stateVal < 16; ++stateVal) {
        JtagTap t(static_cast<JtagTapState>(stateVal));
        t.clockSequence(JtagTap::resetSequence());
        assert(t.currentState() == JtagTapState::TestLogicReset);
    }

    // Verify shortestPath navigation
    JtagTap navTap(JtagTapState::TestLogicReset);
    std::vector<bool> path = navTap.navigateTo(JtagTapState::ShiftDr);
    assert(navTap.currentState() == JtagTapState::ShiftDr);
    // Shortest path from Reset to ShiftDr is: 0 (RunIdle), 1 (SelectDR), 0 (CaptureDR), 0 (ShiftDR) -> length 4
    assert(path.size() == 4);

    path = navTap.navigateTo(JtagTapState::ShiftIr);
    assert(navTap.currentState() == JtagTapState::ShiftIr);

    path = navTap.navigateTo(JtagTapState::RunTestIdle);
    assert(navTap.currentState() == JtagTapState::RunTestIdle);

    std::cout << "  -> PASSED" << std::endl;
}

void testJtagScanData() {
    std::cout << "[TEST] JtagScanData Bit-Precise Manipulations..." << std::endl;

    // Test 5-bit instruction
    JtagScanData ir5 = JtagScanData::fromUint32(0x1F, 5); // 0b11111 (BYPASS)
    assert(ir5.bitCount() == 5);
    for (size_t i = 0; i < 5; ++i) {
        assert(ir5.bit(i) == true);
    }
    assert(ir5.toUint32() == 0x1F);

    // Test non-byte-aligned 37-bit data
    JtagScanData dr37(37);
    dr37.setBit(0, true);
    dr37.setBit(36, true);
    assert(dr37.bit(0) == true);
    assert(dr37.bit(1) == false);
    assert(dr37.bit(36) == true);

    // Test 32-bit IDCODE conversion
    uint32_t rawId = 0x0BC12477;
    JtagScanData idData = JtagScanData::fromUint32(rawId, 32);
    assert(idData.toUint32() == rawId);

    // Test Hex string parsing
    JtagScanData hexData = JtagScanData::fromHex("0xBC12477", 32);
    assert(hexData.toUint32() == rawId);

    std::cout << "  -> PASSED" << std::endl;
}

void testJtagIdCodeAndJep106() {
    std::cout << "[TEST] JtagIdCode & JEP106 Manufacturer Decoding..." << std::endl;

    // RP2040 Cortex-M0+ IDCODE: 0x0BC12477
    JtagIdCode idRP2040(0x0BC12477);
    assert(idRP2040.isValid());
    assert(idRP2040.version() == 0x0);
    assert(idRP2040.partNumber() == 0xBC12);
    // Manufacturer ID: bits [11:1] = 0x23B -> ARM Ltd.
    assert(idRP2040.manufacturerId() == 0x23B);
    assert(idRP2040.manufacturerName() == "ARM Ltd.");

    // Lattice iCE40 IDCODE: 0x020B10DD
    JtagIdCode idLattice(0x020B10DD);
    assert(idLattice.isValid());
    // Manufacturer ID for Lattice: 0x06E or bank entry
    assert(idLattice.manufacturerName() != "");

    // Invalid IDCODEs (all 0s or all 1s or LSB=0)
    JtagIdCode inv0(0x00000000);
    assert(!inv0.isValid());
    JtagIdCode invF(0xFFFFFFFF);
    assert(!invF.isValid());
    JtagIdCode invLsb(0x12345678); // LSB is 0
    assert(!invLsb.isValid());

    std::cout << "  -> PASSED" << std::endl;
}

void testSwdProtocols() {
    std::cout << "[TEST] ARM CoreSight SWD Protocol & DPIDR..." << std::endl;

    // Standard ARM SWD request byte: Read DPIDR (AP=0, RnW=1, Addr=0x00) -> 0xA5
    uint8_t readDpidrHdr = SwdPacket::makeHeader(false, true, DpRegister::IDCODE);
    assert(readDpidrHdr == 0xA5);

    // Write ABORT (AP=0, RnW=0, Addr=0x00) -> 0x81
    uint8_t writeAbortHdr = SwdPacket::makeHeader(false, false, DpRegister::ABORT);
    assert(writeAbortHdr == 0x81);

    // Parity calculations
    assert(SwdPacket::calculateParity32(0x00000001) == true);
    assert(SwdPacket::calculateParity32(0x00000003) == false);
    assert(SwdPacket::verifyParity32(0x0BC12477, SwdPacket::calculateParity32(0x0BC12477)));

    // DPIDR decoding
    SwdDpidr dpidrRP2040(0x0BC12477);
    assert(dpidrRP2040.isValid());
    assert(dpidrRP2040.designerId() == 0x23B);
    assert(dpidrRP2040.designerName() == "ARM Ltd.");
    assert(dpidrRP2040.minDp() == true);
    assert(dpidrRP2040.description().contains("RP2040"));

    std::cout << "  -> PASSED" << std::endl;
}

void testDebugProtocolSerialization() {
    std::cout << "[TEST] DebugProtocol Binary Serialization & CRC16..." << std::endl;

    DebugPacket pReq = DebugProtocol::createRequest(DebugCommand::JTAG_SET_CLOCK, 42, QByteArray::fromHex("000F4240"));
    QByteArray wire = DebugProtocol::serializePacket(pReq);
    assert(wire.size() == 8 + 4);
    assert(static_cast<uint8_t>(wire[0]) == DEBUG_SYNC_BYTE);

    DebugPacket pParsed;
    QString err;
    bool ok = DebugProtocol::parsePacket(wire, pParsed, &err);
    assert(ok);
    assert(pParsed.command == DebugCommand::JTAG_SET_CLOCK);
    assert(pParsed.seq == 42);
    assert(pParsed.status == DebugStatus::OK);
    assert(pParsed.payload == QByteArray::fromHex("000F4240"));

    // Corrupt one byte to verify CRC verification failure
    wire[wire.size() - 1] = static_cast<char>(wire[wire.size() - 1] ^ 0xFF);
    bool corruptOk = DebugProtocol::parsePacket(wire, pParsed, &err);
    assert(!corruptOk);
    assert(err.contains("CRC mismatch"));

    std::cout << "  -> PASSED" << std::endl;
}

void testMockDebugTransportAndClients() {
    std::cout << "[TEST] MockDebugTransport, JtagClient & SwdClient End-to-End..." << std::endl;

    MockDebugTransport transport;
    assert(!transport.isConnected());
    assert(transport.open());
    assert(transport.isConnected());

    // 1. Test JTAG Client
    JtagClient jtag(&transport);

    // Reset TAP
    assert(jtag.resetTap());
    assert(jtag.currentTapState() == JtagTapState::TestLogicReset);

    // Set Clock
    assert(jtag.setClock(5000000)); // 5 MHz

    // Navigate TAP to Shift-DR
    assert(jtag.navigateTap(JtagTapState::ShiftDr));
    assert(jtag.currentTapState() == JtagTapState::ShiftDr);

    // Scan Chain
    auto devs = jtag.scanChain();
    assert(devs.size() == 2);
    assert(devs[0].idcode.raw() == 0x0BC12477); // RP2040 CoreSight
    assert(devs[1].idcode.raw() == 0x020B10DD); // Lattice iCE40

    // Shift-DR 32 bits to read IDCODE
    JtagScanData tdiZeros(32);
    auto drResult = jtag.shiftDr(tdiZeros);
    assert(drResult.success);
    assert(drResult.tdoData.toUint32() == 0x0BC12477);

    // 2. Test SWD Client
    SwdClient swd(&transport);

    // Line Reset
    assert(swd.resetLine());

    // Connect Target
    SwdDpidr dpidr;
    assert(swd.connectTarget(dpidr));
    assert(dpidr.raw() == 0x0BC12477);
    assert(swd.isTargetConnected());

    // Set SWD clock
    assert(swd.setClock(4000000)); // 4 MHz

    // Read DP CTRL/STAT
    auto ctrlStat = swd.readDp(DpRegister::CTRL_STAT);
    assert(ctrlStat.isOk());
    assert(ctrlStat.data == 0xF0000000);

    // Write DP SELECT
    auto selResult = swd.writeDp(DpRegister::SELECT, 0x01000000);
    assert(selResult.isOk());
    assert(transport.simulatedDpRegister(DpRegister::SELECT) == 0x01000000);

    // Read AP IDR
    auto apIdr = swd.readAp(ApRegister::IDR);
    assert(apIdr.isOk());
    assert(apIdr.data == 0x24770011);

    // Write AP TAR & DRW (Simulated Memory Write)
    auto memWrite = swd.writeMem32(0x20001000, 0xDEADBEEF);
    assert(memWrite.isOk());

    transport.close();
    assert(!transport.isConnected());

    std::cout << "  -> PASSED" << std::endl;
}

void testJtagChainMultiDeviceAndTargetedShifts() {
    std::cout << "[TEST] JtagChain Multi-Device Topology & Targeted BYPASS Padding..." << std::endl;

    JtagDevice dev0;
    dev0.chainIndex = 0;
    dev0.idcode = JtagIdCode(0x0BC12477);
    dev0.irLength = 4;
    dev0.name = "RP2040 CoreSight";

    JtagDevice dev1;
    dev1.chainIndex = 1;
    dev1.idcode = JtagIdCode(0x020B10DD);
    dev1.irLength = 6;
    dev1.name = "iCE40 FPGA";

    JtagChain chain({dev0, dev1});
    assert(chain.deviceCount() == 2);
    assert(chain.totalIrLength() == 10);

    // Verify BYPASS instructions (all 1s)
    auto bp0 = chain.bypassInstruction(0);
    assert(bp0.bitCount() == 4);
    assert(bp0.toUint32() == 0x0F);

    auto bp1 = chain.bypassInstruction(1);
    assert(bp1.bitCount() == 6);
    assert(bp1.toUint32() == 0x3F);

    // Verify Standard Instructions
    auto idcode0 = chain.standardInstruction(0, JtagStandardInstruction::IdCode);
    assert(idcode0.bitCount() == 4);
    assert(idcode0.toUint32() == 0x02);

    auto extest1 = chain.standardInstruction(1, JtagStandardInstruction::Extest);
    assert(extest1.bitCount() == 6);
    assert(extest1.toUint32() == 0x00);

    // Test Targeted IR shift for Device 0 (4-bit target: 0b0010):
    // Device 1 (closer to TDO) must receive 6 bits of 1s (BYPASS), Device 0 receives 0b0010.
    JtagScanData targetIr0 = JtagScanData::fromUint32(0x02, 4);
    JtagScanData chainIr0 = chain.prepareTargetedIr(0, targetIr0);
    assert(chainIr0.bitCount() == 10);
    // Bits 0..5 (Dev 1): 111111 (0x3F)
    // Bits 6..9 (Dev 0): 0010 (0x02 << 6 = 0x80)
    // Total: 0x3F | 0x80 = 0x0BF
    assert(chainIr0.toUint32() == 0x0BF);

    // Test Targeted IR extraction for Device 0:
    JtagScanData extractedIr0 = chain.extractTargetedIr(0, chainIr0);
    assert(extractedIr0.bitCount() == 4);
    assert(extractedIr0.toUint32() == 0x02);

    // Test Targeted IR shift for Device 1 (6-bit target: 0b000101):
    // Device 1 receives 6 bits (0x05), Device 0 (closer to TDI) receives 4 bits of 1s (0x0F).
    JtagScanData targetIr1 = JtagScanData::fromUint32(0x05, 6);
    JtagScanData chainIr1 = chain.prepareTargetedIr(1, targetIr1);
    assert(chainIr1.bitCount() == 10);
    // Bits 0..5: 0x05
    // Bits 6..9: 0x0F (0x0F << 6 = 0x3C0)
    // Total: 0x05 | 0x3C0 = 0x3C5
    assert(chainIr1.toUint32() == 0x3C5);

    JtagScanData extractedIr1 = chain.extractTargetedIr(1, chainIr1);
    assert(extractedIr1.bitCount() == 6);
    assert(extractedIr1.toUint32() == 0x05);

    // Test Targeted DR shift for Device 0 (32 bits data):
    // Device 1 is in BYPASS (1 bit pad at bit 0). Device 0 data at bits 1..32.
    JtagScanData drData0 = JtagScanData::fromUint32(0xA5A5A5A5, 32);
    JtagScanData chainDr0 = chain.prepareTargetedDr(0, drData0);
    assert(chainDr0.bitCount() == 33); // 1 bypass bit + 32 data bits
    assert(chainDr0.bit(0) == false);  // bypass pad is 0
    assert(chainDr0.bit(1) == true);   // LSB of 0xA5A5A5A5 (bit 0 is 1)

    JtagScanData extractedDr0 = chain.extractTargetedDr(0, chainDr0, 32);
    assert(extractedDr0.bitCount() == 32);
    assert(extractedDr0.toUint32() == 0xA5A5A5A5);

    std::cout << "  -> PASSED" << std::endl;
}

void testJtagCaptureIrPatternParsing() {
    std::cout << "[TEST] IEEE 1149.1 Capture-IR '...01' Pattern Discovery..." << std::endl;

    // Simulate Capture-IR scan on chain of 2 devices:
    // Dev 1 (closer to TDO): 6-bit IR, captures 0b000001 (0x01)
    // Dev 0 (closer to TDI): 4-bit IR, captures 0b0001   (0x01)
    // Composite 10-bit stream at TDO: 0x01 | (0x01 << 6) = 0x0041
    JtagScanData capData = JtagScanData::fromUint32(0x0041, 10);

    std::vector<size_t> irLens;
    bool parsed = JtagChain::parseCaptureIrPattern(capData, 10, 2, irLens);
    assert(parsed);
    assert(irLens.size() == 2);
    assert(irLens[0] == 4); // Dev 0
    assert(irLens[1] == 6); // Dev 1

    std::cout << "  -> PASSED" << std::endl;
}

void testJtagClientTargetedAndBatchOperations() {
    std::cout << "[TEST] JtagClient Multi-Device Targeted Shifts & Deterministic Batch..." << std::endl;

    MockDebugTransport transport;
    assert(transport.open());
    JtagClient jtag(&transport);

    // 1. Scan chain and auto-discover IR lengths
    auto devs = jtag.scanChain();
    assert(devs.size() == 2);
    assert(devs[0].irLength == 4);
    assert(devs[1].irLength == 6);
    assert(jtag.chain().totalIrLength() == 10);

    // 2. Targeted Device 0: Shift standard IDCODE instruction, then targeted DR shift
    auto irRes0 = jtag.shiftDeviceIr(0, JtagScanData::fromUint32(0x01, 4));
    assert(irRes0.success);

    // Targeted DR shift to read Device 0's 32-bit IDCODE (with 1-bit bypass padding for Dev 1)
    auto drRes0 = jtag.shiftDeviceDr(0, JtagScanData(32), 32);
    assert(drRes0.success);
    assert(drRes0.tdoData.toUint32() == 0x0BC12477);

    // 3. Targeted Device 1: Shift standard IDCODE instruction, then targeted DR shift
    auto irRes1 = jtag.shiftDeviceIr(1, JtagScanData::fromUint32(0x01, 6));
    assert(irRes1.success);

    auto drRes1 = jtag.shiftDeviceDr(1, JtagScanData(32), 32);
    assert(drRes1.success);
    assert(drRes1.tdoData.toUint32() == 0x020B10DD);

    // 4. Deterministic Batch Transaction Sequence
    std::vector<JtagBatchOperation> batch;

    JtagBatchOperation opReset;
    opReset.type = JtagOpType::TapReset;
    batch.push_back(opReset);

    JtagBatchOperation opNav;
    opNav.type = JtagOpType::GotoState;
    opNav.targetState = JtagTapState::ShiftDr;
    batch.push_back(opNav);

    JtagBatchOperation opShift;
    opShift.type = JtagOpType::ShiftDr;
    opShift.data = JtagScanData(32);
    opShift.targetState = JtagTapState::RunTestIdle;
    batch.push_back(opShift);

    JtagBatchOperation opIdle;
    opIdle.type = JtagOpType::ClockIdle;
    opIdle.clockCycles = 16;
    batch.push_back(opIdle);

    auto batchRes = jtag.executeBatch(batch);
    assert(batchRes.success);
    assert(batchRes.finalState == JtagTapState::RunTestIdle);
    assert(batchRes.shiftResults.size() == 1);
    assert(batchRes.shiftResults[0].toUint32() == 0x0BC12477);

    transport.close();
    std::cout << "  -> PASSED" << std::endl;
}

bool runJtagSwdTests() {
    try {
        testJtagTapTransitions();
        testJtagScanData();
        testJtagIdCodeAndJep106();
        testSwdProtocols();
        testDebugProtocolSerialization();
        testMockDebugTransportAndClients();
        testJtagChainMultiDeviceAndTargetedShifts();
        testJtagCaptureIrPatternParsing();
        testJtagClientTargetedAndBatchOperations();
        return true;
    } catch (...) {
        return false;
    }
}

