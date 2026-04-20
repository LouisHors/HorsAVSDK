#include <gtest/gtest.h>
#include "avsdk/hls_parser.h"
#include <sstream>

using namespace avsdk;

TEST(HlsParserTest, ParseSimplePlaylist) {
    std::string m3u8 = R"(#EXTM3U
#EXT-X-VERSION:3
#EXT-X-TARGETDURATION:10
#EXTINF:9.009,
segment1.ts
#EXTINF:9.009,
segment2.ts
#EXTINF:5.005,
segment3.ts
#EXT-X-ENDLIST
)";

    HlsPlaylist playlist;
    bool result = HlsParser::Parse(m3u8, playlist);
    EXPECT_TRUE(result);
    EXPECT_EQ(playlist.version, 3);
    EXPECT_EQ(playlist.target_duration, 10);
    EXPECT_EQ(playlist.segments.size(), 3);
    EXPECT_EQ(playlist.segments[0].duration, 9.009);
    EXPECT_EQ(playlist.segments[0].url, "segment1.ts");
}

TEST(HlsParserTest, ParseMasterPlaylist) {
    std::string m3u8 = R"(#EXTM3U
#EXT-X-STREAM-INF:BANDWIDTH=1280000,RESOLUTION=640x480
low.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=2560000,RESOLUTION=1280x720
mid.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=7680000,RESOLUTION=1920x1080
hi.m3u8
)";

    HlsPlaylist playlist;
    bool result = HlsParser::Parse(m3u8, playlist);
    EXPECT_TRUE(result);
    EXPECT_EQ(playlist.variants.size(), 3);
    EXPECT_EQ(playlist.variants[0].bandwidth, 1280000);
    EXPECT_EQ(playlist.variants[0].width, 640);
    EXPECT_EQ(playlist.variants[0].height, 480);
}

TEST(HlsParserTest, ParseLivePlaylist) {
    std::string m3u8 = R"(#EXTM3U
#EXT-X-VERSION:3
#EXT-X-TARGETDURATION:10
#EXT-X-MEDIA-SEQUENCE:0
#EXTINF:9.009,
segment0.ts
#EXTINF:9.009,
segment1.ts
)";

    HlsPlaylist playlist;
    bool result = HlsParser::Parse(m3u8, playlist);
    EXPECT_TRUE(result);
    EXPECT_EQ(playlist.media_sequence, 0);
    EXPECT_FALSE(playlist.end_list);
}
