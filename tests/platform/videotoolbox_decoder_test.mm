#include <gtest/gtest.h>
#include "platform/macos/videotoolbox_decoder.h"
#include "avsdk/error.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

using namespace avsdk;

TEST(VideoToolboxDecoderTest, CreateInstance) {
    auto decoder = CreateVideoToolboxDecoder();
    EXPECT_NE(decoder, nullptr);
}

TEST(VideoToolboxDecoderTest, InitializeWithH264) {
    auto decoder = CreateVideoToolboxDecoder();

    // Create mock codec parameters for H264
    AVCodecParameters* codecpar = avcodec_parameters_alloc();
    codecpar->codec_id = AV_CODEC_ID_H264;
    codecpar->width = 1920;
    codecpar->height = 1080;

    auto result = decoder->Initialize(codecpar);
    EXPECT_EQ(result, ErrorCode::OK);

    avcodec_parameters_free(&codecpar);
}

TEST(VideoToolboxDecoderTest, InitializeWithH265) {
    auto decoder = CreateVideoToolboxDecoder();

    // Create mock codec parameters for HEVC
    AVCodecParameters* codecpar = avcodec_parameters_alloc();
    codecpar->codec_id = AV_CODEC_ID_HEVC;
    codecpar->width = 1920;
    codecpar->height = 1080;

    auto result = decoder->Initialize(codecpar);
    EXPECT_EQ(result, ErrorCode::OK);

    avcodec_parameters_free(&codecpar);
}
