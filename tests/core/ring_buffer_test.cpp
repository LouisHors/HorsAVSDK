#include <gtest/gtest.h>
#include "avsdk/ring_buffer.h"
#include <vector>
#include <thread>

using namespace avsdk;

TEST(RingBufferTest, BasicWriteRead) {
    RingBuffer buffer(1024);

    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    size_t written = buffer.Write(data.data(), data.size());
    EXPECT_EQ(written, 5);

    std::vector<uint8_t> read(5);
    size_t read_count = buffer.Read(read.data(), read.size());
    EXPECT_EQ(read_count, 5);
    EXPECT_EQ(read, data);
}

TEST(RingBufferTest, CircularWrite) {
    RingBuffer buffer(16);

    // Fill buffer
    std::vector<uint8_t> data1(12, 0xAA);
    buffer.Write(data1.data(), data1.size());

    // Read some
    std::vector<uint8_t> read1(8);
    buffer.Read(read1.data(), read1.size());

    // Write more (should wrap around)
    std::vector<uint8_t> data2(8, 0xBB);
    size_t written = buffer.Write(data2.data(), data2.size());
    EXPECT_EQ(written, 8);

    // Read remaining
    std::vector<uint8_t> read2(12);
    size_t read_count = buffer.Read(read2.data(), read2.size());
    EXPECT_EQ(read_count, 12);
}

TEST(RingBufferTest, ConcurrentReadWrite) {
    RingBuffer buffer(4096);
    std::atomic<bool> stop{false};
    std::atomic<size_t> total_written{0};
    std::atomic<size_t> total_read{0};

    // Writer thread
    std::thread writer([&]() {
        std::vector<uint8_t> data(256, 0x42);
        while (!stop) {
            size_t written = buffer.Write(data.data(), data.size());
            total_written += written;
            if (written < data.size()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });

    // Reader thread
    std::thread reader([&]() {
        std::vector<uint8_t> data(256);
        while (!stop) {
            size_t read = buffer.Read(data.data(), data.size());
            total_read += read;
            if (read == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;

    writer.join();
    reader.join();

    // Drain remaining
    std::vector<uint8_t> drain(4096);
    total_read += buffer.Read(drain.data(), drain.size());

    EXPECT_EQ(total_written.load(), total_read.load());
}
