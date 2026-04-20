#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "avsdk/player.h"
#include "avsdk/player_config.h"
#include "avsdk/error.h"

using namespace avsdk;

TEST(PlayerIntegrationTest, PlayLocalVideo) {
    // Create 5-second test video
    system("ffmpeg -f lavfi -i testsrc=duration=5:size=640x480:rate=30 -pix_fmt yuv420p -c:v libx264 integration_test.mp4 -y");

    auto player = CreatePlayer();
    ASSERT_NE(player, nullptr);

    PlayerConfig config;
    config.enable_hardware_decoder = false;
    config.buffer_time_ms = 500;

    auto result = player->Initialize(config);
    EXPECT_EQ(result, ErrorCode::OK);

    result = player->Open("integration_test.mp4");
    EXPECT_EQ(result, ErrorCode::OK);

    auto info = player->GetMediaInfo();
    EXPECT_TRUE(info.has_video);
    EXPECT_EQ(info.video_width, 640);
    EXPECT_EQ(info.video_height, 480);
    EXPECT_EQ(info.duration_ms, 5000);

    // Play for 1 second
    player->Play();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(player->GetState(), PlayerState::kPlaying);

    player->Stop();
    EXPECT_EQ(player->GetState(), PlayerState::kStopped);
}
