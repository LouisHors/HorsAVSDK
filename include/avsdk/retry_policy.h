#pragma once
#include <functional>
#include <chrono>

namespace avsdk {

struct RetryPolicy {
    int max_retries = 3;
    int base_delay_ms = 1000;
    int max_delay_ms = 30000;
    bool use_exponential_backoff = true;
    float backoff_multiplier = 2.0f;

    // Execute function with retry logic
    bool Execute(std::function<bool()> func);

    // Calculate delay for specific retry attempt
    int CalculateDelay(int attempt) const;

    // Reset state
    void Reset();

private:
    int current_attempt_ = 0;
};

} // namespace avsdk
