#include <gtest/gtest.h>
#include "avsdk/memory_pool.h"

using namespace avsdk;

TEST(MemoryPoolTest, AllocateAndFree) {
    void* ptr = MemoryPool::Allocate(1024);
    EXPECT_NE(ptr, nullptr);
    MemoryPool::Free(ptr, 1024);
}

TEST(MemoryPoolTest, AllocateDifferentSizes) {
    void* small = MemoryPool::Allocate(256);
    void* medium = MemoryPool::Allocate(4096);
    void* large = MemoryPool::Allocate(65536);

    EXPECT_NE(small, nullptr);
    EXPECT_NE(medium, nullptr);
    EXPECT_NE(large, nullptr);

    MemoryPool::Free(small, 256);
    MemoryPool::Free(medium, 4096);
    MemoryPool::Free(large, 65536);
}

TEST(MemoryPoolTest, StatsTracking) {
    MemoryPool::Clear();

    void* ptr1 = MemoryPool::Allocate(1024);
    void* ptr2 = MemoryPool::Allocate(2048);

    auto stats = MemoryPool::GetStats();
    EXPECT_EQ(stats.allocation_count, 2);

    MemoryPool::Free(ptr1, 1024);
    MemoryPool::Free(ptr2, 2048);
}
