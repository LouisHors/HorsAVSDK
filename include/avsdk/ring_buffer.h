#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace avsdk {

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);
    ~RingBuffer();

    // Write data, returns actual bytes written
    size_t Write(const uint8_t* data, size_t size);

    // Read data, returns actual bytes read
    size_t Read(uint8_t* data, size_t size);

    // Get available bytes to read
    size_t AvailableToRead() const;

    // Get available space to write
    size_t AvailableToWrite() const;

    // Clear buffer
    void Clear();

    // Wait until data available or timeout
    bool WaitForData(int timeout_ms);

private:
    std::vector<uint8_t> buffer_;
    size_t capacity_;
    size_t read_pos_ = 0;
    size_t write_pos_ = 0;
    size_t size_ = 0;

    mutable std::mutex mutex_;
    std::condition_variable data_available_;
    std::condition_variable space_available_;
};

} // namespace avsdk
