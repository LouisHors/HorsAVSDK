#include <gtest/gtest.h>
#include "platform/macos/videotoolbox_decoder.h"
#include "avsdk/error.h"

using namespace avsdk;

TEST(VideoToolboxDecoderTest, CreateInstance) {
    auto decoder = CreateVideoToolboxDecoder();
    EXPECT_NE(decoder, nullptr);
}

TEST(VideoToolboxDecoderTest, InitializeWithH264) {
    auto decoder = CreateVideoToolboxDecoder();

    auto result = decoder->Initialize(CodecType::H264, 1920, 1080);
    EXPECT_EQ(result, ErrorCode::OK);
}

TEST(VideoToolboxDecoderTest, InitializeWithH265) {
    auto decoder = CreateVideoToolboxDecoder();

    auto result = decoder->Initialize(CodecType::H265, 1920, 1080);
    EXPECT_EQ(result, ErrorCode::OK);
}
