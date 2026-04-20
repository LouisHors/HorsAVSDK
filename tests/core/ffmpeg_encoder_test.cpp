#include <gtest/gtest.h>
#include "avsdk/encoder.h"
#include "avsdk/error.h"

using namespace avsdk;

TEST(FFmpegEncoderTest, CreateInstance) {
    auto encoder = CreateFFmpegEncoder();
    EXPECT_NE(encoder, nullptr);
}

TEST(FFmpegEncoderTest, InitializeH264) {
    auto encoder = CreateFFmpegEncoder();

    EncoderConfig config;
    config.codec = EncoderCodec::H264;
    config.width = 640;
    config.height = 480;
    config.bitrate = 1000000;
    config.frame_rate = 30;

    auto result = encoder->Initialize(config);
    EXPECT_EQ(result, ErrorCode::OK);
}

TEST(FFmpegEncoderTest, InitializeH265) {
    auto encoder = CreateFFmpegEncoder();

    EncoderConfig config;
    config.codec = EncoderCodec::H265;
    config.width = 1280;
    config.height = 720;
    config.bitrate = 2000000;
    config.frame_rate = 30;

    auto result = encoder->Initialize(config);
    EXPECT_EQ(result, ErrorCode::OK);
}
