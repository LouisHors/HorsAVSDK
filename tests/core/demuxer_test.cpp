#include <gtest/gtest.h>
#include "avsdk/demuxer.h"
#include "avsdk/error.h"
#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
}

using namespace avsdk;

class DemuxerTest : public ::testing::Test {
protected:
    void SetUp() override {
        demuxer_ = CreateFFmpegDemuxer();
    }

    std::unique_ptr<IDemuxer> demuxer_;
};

TEST_F(DemuxerTest, OpenNonExistentFile) {
    auto result = demuxer_->Open("/nonexistent/file.mp4");
    EXPECT_EQ(result, ErrorCode::FileNotFound);
}

TEST_F(DemuxerTest, OpenValidFile) {
    // Create a test video file using ffmpeg
    system("ffmpeg -f lavfi -i testsrc=duration=1:size=320x240:rate=1 -pix_fmt yuv420p test_video.mp4 -y");

    auto result = demuxer_->Open("test_video.mp4");
    EXPECT_EQ(result, ErrorCode::OK);

    auto info = demuxer_->GetMediaInfo();
    EXPECT_TRUE(info.has_video);
    EXPECT_EQ(info.video_width, 320);
    EXPECT_EQ(info.video_height, 240);
}

TEST_F(DemuxerTest, ReadPacket) {
    system("ffmpeg -f lavfi -i testsrc=duration=1:size=320x240:rate=1 -pix_fmt yuv420p test_video.mp4 -y");

    demuxer_->Open("test_video.mp4");

    AVPacketPtr packet = demuxer_->ReadPacket();
    EXPECT_NE(packet, nullptr);
    EXPECT_GT(packet->size, 0);
}
