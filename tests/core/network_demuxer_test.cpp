#include <gtest/gtest.h>
#include "avsdk/demuxer.h"
#include "avsdk/error.h"

using namespace avsdk;

TEST(NetworkDemuxerTest, CreateDemuxer) {
    auto demuxer = CreateNetworkDemuxer();
    EXPECT_NE(demuxer, nullptr);
}

TEST(NetworkDemuxerTest, GetMediaInfoReturnsEmpty) {
    auto demuxer = CreateNetworkDemuxer();
    auto info = demuxer->GetMediaInfo();
    EXPECT_FALSE(info.has_video);
    EXPECT_FALSE(info.has_audio);
    EXPECT_EQ(info.duration_ms, 0);
}
