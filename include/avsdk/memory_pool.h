#pragma once
#include <cstddef>
#include <cstdint>

namespace avsdk {

class MemoryPool {
public:
    struct Stats {
        size_t total_allocated;
        size_t total_used;
        size_t allocation_count;
        size_t deallocation_count;
    };

    static void* Allocate(size_t size);
    static void Free(void* ptr, size_t size);
    static void Clear();
    static Stats GetStats();
};

} // namespace avsdk
