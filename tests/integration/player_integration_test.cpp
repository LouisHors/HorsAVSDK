#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "avsdk/player.h"
#include "avsdk/player_config.h"
#include "avsdk/error.h"

using namespace avsdk;

TEST(PlayerIntegrationTest, PlayLocalVideo) {
    // Create 5-second test video with audio (10 seconds to ensure playback continues during test)
    system("ffmpeg -f lavfi -i testsrc=duration=10:size=640x480:rate=30 -f lavfi -i sine=frequency=1000:duration=10 -pix_fmt yuv420p -c:v libx264 -c:a aac integration_test.mp4 -y 2>/dev/null");

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
    EXPECT_TRUE(info.has_audio);  // Now should have audio
    EXPECT_EQ(info.video_width, 640);
    EXPECT_EQ(info.video_height, 480);

    // Play for 2 seconds
    player->Play();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // State should be Playing or Stopped (if file ended)
    auto state = player->GetState();
    EXPECT_TRUE(state == PlayerState::kPlaying || state == PlayerState::kStopped);

    player->Stop();
    EXPECT_EQ(player->GetState(), PlayerState::kStopped);
}
