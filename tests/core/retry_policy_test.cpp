#include <gtest/gtest.h>
#include "avsdk/retry_policy.h"

using namespace avsdk;

TEST(RetryPolicyTest, ShouldRetryOnFailure) {
    RetryPolicy policy;
    policy.max_retries = 3;
    policy.base_delay_ms = 100;

    int attempt = 0;
    bool result = policy.Execute([&]() -> bool {
        attempt++;
        return attempt >= 3; // Succeed on 3rd attempt
    });

    EXPECT_TRUE(result);
    EXPECT_EQ(attempt, 3);
}

TEST(RetryPolicyTest, ExponentialBackoff) {
    RetryPolicy policy;
    policy.max_retries = 3;
    policy.base_delay_ms = 100;
    policy.use_exponential_backoff = true;

    auto start = std::chrono::steady_clock::now();

    int attempt = 0;
    policy.Execute([&]() -> bool {
        attempt++;
        return attempt >= 3;
    });

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should have delays: 100ms + 200ms = 300ms minimum
    EXPECT_GE(duration.count(), 250);
}

TEST(RetryPolicyTest, MaxRetriesExceeded) {
    RetryPolicy policy;
    policy.max_retries = 2;

    int attempt = 0;
    bool result = policy.Execute([&]() -> bool {
        attempt++;
        return false; // Always fail
    });

    EXPECT_FALSE(result);
    EXPECT_EQ(attempt, 3); // Initial + 2 retries
}
