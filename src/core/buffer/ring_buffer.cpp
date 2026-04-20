#include "avsdk/ring_buffer.h"
#include <algorithm>

namespace avsdk {

RingBuffer::RingBuffer(size_t capacity)
    : buffer_(capacity), capacity_(capacity) {}

RingBuffer::~RingBuffer() = default;

size_t RingBuffer::Write(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t to_write = std::min(size, capacity_ - size_);
    size_t written = 0;

    while (written < to_write) {
        size_t pos = write_pos_ % capacity_;
        size_t chunk = std::min(to_write - written, capacity_ - pos);
        std::copy(data + written, data + written + chunk, buffer_.begin() + pos);
        write_pos_ += chunk;
        written += chunk;
    }

    size_ += written;

    if (written > 0) {
        data_available_.notify_one();
    }

    return written;
}

size_t RingBuffer::Read(uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t to_read = std::min(size, size_);
    size_t read = 0;

    while (read < to_read) {
        size_t pos = read_pos_ % capacity_;
        size_t chunk = std::min(to_read - read, capacity_ - pos);
        std::copy(buffer_.begin() + pos, buffer_.begin() + pos + chunk, data + read);
        read_pos_ += chunk;
        read += chunk;
    }

    size_ -= read;

    if (read > 0) {
        space_available_.notify_one();
    }

    return read;
}

size_t RingBuffer::AvailableToRead() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

size_t RingBuffer::AvailableToWrite() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return capacity_ - size_;
}

void RingBuffer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    read_pos_ = 0;
    write_pos_ = 0;
    size_ = 0;
}

bool RingBuffer::WaitForData(int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    return data_available_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                    [this] { return size_ > 0; });
}

} // namespace avsdk
