#pragma once
#include <string>
#include <functional>
#include <mutex>

namespace avsdk {

enum class LogLevel {
    kVerbose = 0,
    kDebug = 1,
    kInfo = 2,
    kWarning = 3,
    kError = 4,
    kFatal = 5
};

class Logger {
public:
    static Logger& GetInstance();

    void SetLogLevel(LogLevel level);
    LogLevel GetLogLevel() const;

    void SetCallback(std::function<void(LogLevel, const std::string&)> callback);
    void Log(LogLevel level, const std::string& tag, const std::string& message);

private:
    Logger() = default;
    LogLevel level_ = LogLevel::kInfo;
    std::function<void(LogLevel, const std::string&)> callback_;
    mutable std::mutex mutex_;
};

#define LOG_INFO(tag, msg) Logger::GetInstance().Log(LogLevel::kInfo, tag, msg)
#define LOG_ERROR(tag, msg) Logger::GetInstance().Log(LogLevel::kError, tag, msg)

} // namespace avsdk
