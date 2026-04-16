#include <gtest/gtest.h>
#include "avsdk/logger.h"

using namespace avsdk;

TEST(LoggerTest, SingletonInstance) {
    Logger& logger1 = Logger::GetInstance();
    Logger& logger2 = Logger::GetInstance();
    EXPECT_EQ(&logger1, &logger2);
}

TEST(LoggerTest, SetLogLevel) {
    Logger& logger = Logger::GetInstance();
    logger.SetLogLevel(LogLevel::kDebug);
    EXPECT_EQ(logger.GetLogLevel(), LogLevel::kDebug);
}

TEST(LoggerTest, LogMessage) {
    Logger& logger = Logger::GetInstance();
    logger.SetLogLevel(LogLevel::kInfo);

    std::string captured;
    logger.SetCallback([&captured](LogLevel level, const std::string& msg) {
        captured = msg;
    });

    logger.Log(LogLevel::kInfo, "TEST", "Hello World");
    EXPECT_NE(captured.find("Hello World"), std::string::npos);
}
