#include "RingBuffer.h"
#include <iostream>
#include <vector>

bool runRingBufferTests() {
    using namespace UdooKey;
    RingBuffer<uint8_t, 16> rb;

    if (!rb.empty()) return false;
    if (rb.size() != 0) return false;

    // Push elements
    for (uint8_t i = 1; i <= 10; ++i) {
        if (!rb.push(i)) return false;
    }

    if (rb.size() != 10) return false;

    // Pop 5 elements
    for (uint8_t i = 1; i <= 5; ++i) {
        uint8_t val = 0;
        if (!rb.pop(val)) return false;
        if (val != i) return false;
    }

    if (rb.size() != 5) return false;

    // Write chunk that wraps around
    uint8_t chunkIn[8] = {11, 12, 13, 14, 15, 16, 17, 18};
    size_t written = rb.write(chunkIn, 8);
    if (written != 8) return false;

    if (rb.size() != 13) return false;

    // Read all
    uint8_t chunkOut[16] = {0};
    size_t readCount = rb.read(chunkOut, 16);
    if (readCount != 13) return false;

    // First 5 elements were 6..10
    for (int i = 0; i < 5; ++i) {
        if (chunkOut[i] != 6 + i) return false;
    }
    // Next 8 elements were 11..18
    for (int i = 0; i < 8; ++i) {
        if (chunkOut[5 + i] != 11 + i) return false;
    }

    if (!rb.empty()) return false;

    return true;
}
