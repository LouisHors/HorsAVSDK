#include <gtest/gtest.h>
#include "avsdk/player.h"
#include "avsdk/player_config.h"
#include "avsdk/error.h"

using namespace avsdk;

TEST(PlayerTest, CreateAndInitialize) {
    auto player = CreatePlayer();
    EXPECT_NE(player, nullptr);

    PlayerConfig config;
    auto result = player->Initialize(config);
    EXPECT_EQ(result, ErrorCode::OK);
}

TEST(PlayerTest, OpenFile) {
    system("ffmpeg -f lavfi -i testsrc=duration=1:size=320x240:rate=1 -pix_fmt yuv420p test.mp4 -y");

    auto player = CreatePlayer();
    player->Initialize(PlayerConfig{});

    auto result = player->Open("test.mp4");
    EXPECT_EQ(result, ErrorCode::OK);

    auto info = player->GetMediaInfo();
    EXPECT_TRUE(info.has_video);
    EXPECT_EQ(info.video_width, 320);
    EXPECT_EQ(info.video_height, 240);
}

TEST(PlayerTest, PlayPauseStop) {
    system("ffmpeg -f lavfi -i testsrc=duration=3:size=320x240:rate=1 -pix_fmt yuv420p test.mp4 -y");

    auto player = CreatePlayer();
    player->Initialize(PlayerConfig{});
    player->Open("test.mp4");

    EXPECT_EQ(player->GetState(), PlayerState::kStopped);

    player->Play();
    EXPECT_EQ(player->GetState(), PlayerState::kPlaying);

    player->Pause();
    EXPECT_EQ(player->GetState(), PlayerState::kPaused);

    player->Stop();
    EXPECT_EQ(player->GetState(), PlayerState::kStopped);
}
