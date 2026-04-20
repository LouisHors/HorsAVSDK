#include "avsdk/memory_pool.h"
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <map>

namespace avsdk {

static std::mutex g_mutex;
static std::map<void*, size_t> g_allocations;
static size_t g_total_allocated = 0;

void* MemoryPool::Allocate(size_t size) {
    std::lock_guard<std::mutex> lock(g_mutex);
    void* ptr = aligned_alloc(64, size);
    if (ptr) {
        g_allocations[ptr] = size;
        g_total_allocated += size;
    }
    return ptr;
}

void MemoryPool::Free(void* ptr, size_t size) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (ptr && g_allocations.count(ptr)) {
        g_total_allocated -= g_allocations[ptr];
        g_allocations.erase(ptr);
    }
    std::free(ptr);
}

void MemoryPool::Clear() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_allocations.clear();
    g_total_allocated = 0;
}

MemoryPool::Stats MemoryPool::GetStats() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Stats stats{};
    stats.total_allocated = g_total_allocated;
    stats.allocation_count = g_allocations.size();
    return stats;
}

} // namespace avsdk
