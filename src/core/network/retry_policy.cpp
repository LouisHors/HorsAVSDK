#include "avsdk/retry_policy.h"
#include "avsdk/logger.h"
#include <thread>
#include <algorithm>
#include <cmath>

namespace avsdk {

bool RetryPolicy::Execute(std::function<bool()> func) {
    current_attempt_ = 0;

    while (current_attempt_ <= max_retries) {
        if (func()) {
            return true;
        }

        if (current_attempt_ < max_retries) {
            int delay = CalculateDelay(current_attempt_);
            LOG_INFO("RetryPolicy", "Attempt " + std::to_string(current_attempt_ + 1) +
                     " failed, retrying in " + std::to_string(delay) + "ms");
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }

        current_attempt_++;
    }

    LOG_ERROR("RetryPolicy", "All " + std::to_string(max_retries + 1) + " attempts failed");
    return false;
}

int RetryPolicy::CalculateDelay(int attempt) const {
    if (!use_exponential_backoff) {
        return base_delay_ms;
    }

    float delay = base_delay_ms * std::pow(backoff_multiplier, attempt);
    return static_cast<int>(std::min(delay, static_cast<float>(max_delay_ms)));
}

void RetryPolicy::Reset() {
    current_attempt_ = 0;
}

} // namespace avsdk
