#include "avsdk/logger.h"
#include <iostream>

namespace avsdk {

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

void Logger::SetLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

LogLevel Logger::GetLogLevel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return level_;
}

void Logger::SetCallback(std::function<void(LogLevel, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
}

void Logger::Log(LogLevel level, const std::string& tag, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level < level_) return;

    if (callback_) {
        callback_(level, "[" + tag + "] " + message);
    } else {
        std::cout << "[" << tag << "] " << message << std::endl;
    }
}

} // namespace avsdk
