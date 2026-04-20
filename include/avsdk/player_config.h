#pragma once
#include <string>

namespace avsdk {

enum class PlayerState {
    kIdle,
    kStopped,
    kPlaying,
    kPaused,
    kError
};

struct PlayerConfig {
    bool enable_hardware_decoder = false;
    int buffer_time_ms = 1000;
    std::string log_level = "info";
};

} // namespace avsdk
