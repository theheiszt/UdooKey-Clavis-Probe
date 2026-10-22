#pragma once

#include <vector>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <algorithm>

namespace UdooKey {

template <typename T, size_t Capacity = 1024 * 1024>
class RingBuffer {
public:
    RingBuffer() : m_head(0), m_tail(0) {
        m_buffer.resize(Capacity);
    }

    void clear() {
        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_relaxed);
    }

    size_t size() const {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t tail = m_tail.load(std::memory_order_relaxed);
        if (head >= tail) {
            return head - tail;
        }
        return Capacity - (tail - head);
    }

    size_t capacity() const {
        return Capacity - 1; // 1 slot reserved to distinguish full from empty
    }

    size_t freeSpace() const {
        return capacity() - size();
    }

    bool empty() const {
        return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed);
    }

    bool push(const T& item) {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t nextHead = (head + 1) % Capacity;
        if (nextHead == m_tail.load(std::memory_order_acquire)) {
            return false; // Full
        }
        m_buffer[head] = item;
        m_head.store(nextHead, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire)) {
            return false; // Empty
        }
        item = m_buffer[tail];
        m_tail.store((tail + 1) % Capacity, std::memory_order_release);
        return true;
    }

    size_t write(const T* data, size_t count) {
        if (!data || count == 0) return 0;
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t tail = m_tail.load(std::memory_order_acquire);
        
        size_t available;
        if (head >= tail) {
            available = (Capacity - 1) - (head - tail);
        } else {
            available = tail - head - 1;
        }

        size_t toWrite = std::min(count, available);
        if (toWrite == 0) return 0;

        size_t firstChunk = std::min(toWrite, Capacity - head);
        std::memcpy(&m_buffer[head], data, firstChunk * sizeof(T));
        if (toWrite > firstChunk) {
            std::memcpy(&m_buffer[0], data + firstChunk, (toWrite - firstChunk) * sizeof(T));
        }

        m_head.store((head + toWrite) % Capacity, std::memory_order_release);
        return toWrite;
    }

    size_t read(T* dest, size_t maxCount) {
        if (!dest || maxCount == 0) return 0;
        size_t tail = m_tail.load(std::memory_order_relaxed);
        size_t head = m_head.load(std::memory_order_acquire);

        size_t available;
        if (head >= tail) {
            available = head - tail;
        } else {
            available = Capacity - (tail - head);
        }

        size_t toRead = std::min(maxCount, available);
        if (toRead == 0) return 0;

        size_t firstChunk = std::min(toRead, Capacity - tail);
        std::memcpy(dest, &m_buffer[tail], firstChunk * sizeof(T));
        if (toRead > firstChunk) {
            std::memcpy(dest + firstChunk, &m_buffer[0], (toRead - firstChunk) * sizeof(T));
        }

        m_tail.store((tail + toRead) % Capacity, std::memory_order_release);
        return toRead;
    }

private:
    std::vector<T> m_buffer;
    std::atomic<size_t> m_head;
    std::atomic<size_t> m_tail;
};

} // namespace UdooKey
