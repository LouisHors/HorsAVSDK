#include <gtest/gtest.h>
#include "avsdk/player.h"
#include "avsdk/player_config.h"
#include "avsdk/data_bypass.h"
#include "avsdk/error.h"

using namespace avsdk;

// Test implementation of IDataBypass
class TestDataBypass : public IDataBypass {
public:
    int rawPacketCount = 0;
    int decodedVideoCount = 0;
    int decodedAudioCount = 0;
    int preRenderVideoCount = 0;
    int preRenderAudioCount = 0;
    int encodedPacketCount = 0;

    void OnRawPacket(const EncodedPacket& packet) override {
        rawPacketCount++;
    }

    void OnDecodedVideoFrame(const VideoFrame& frame) override {
        decodedVideoCount++;
    }

    void OnDecodedAudioFrame(const AudioFrame& frame) override {
        decodedAudioCount++;
    }

    void OnPreRenderVideoFrame(VideoFrame& frame) override {
        preRenderVideoCount++;
    }

    void OnPreRenderAudioFrame(AudioFrame& frame) override {
        preRenderAudioCount++;
    }

    void OnEncodedPacket(const EncodedPacket& packet) override {
        encodedPacketCount++;
    }
};

// Test 1: Verify SetDataBypass works
TEST(PlayerBypassTest, SetDataBypass) {
    auto player = CreatePlayer();
    ASSERT_NE(player, nullptr);

    PlayerConfig config;
    auto result = player->Initialize(config);
    EXPECT_EQ(result, ErrorCode::OK);

    auto bypass = std::make_shared<TestDataBypass>();
    ASSERT_NE(bypass, nullptr);

    // Test SetDataBypass returns OK
    result = player->SetDataBypass(bypass);
    EXPECT_EQ(result, ErrorCode::OK);

    // Verify the bypass is registered by checking manager has it
    auto manager = player->GetDataBypassManager();
    ASSERT_NE(manager, nullptr);
    EXPECT_EQ(manager->GetBypassCount(), 1);
}

// Test 2: Verify GetDataBypassManager returns non-null
TEST(PlayerBypassTest, GetDataBypassManager) {
    auto player = CreatePlayer();
    ASSERT_NE(player, nullptr);

    // Test GetDataBypassManager creates manager on first call
    auto manager = player->GetDataBypassManager();
    EXPECT_NE(manager, nullptr);

    // Test it's the same manager on subsequent calls
    auto manager2 = player->GetDataBypassManager();
    EXPECT_EQ(manager, manager2);
}

// Test 3: Verify EnableVideoFrameCallback works
TEST(PlayerBypassTest, EnableVideoFrameCallback) {
    auto player = CreatePlayer();
    ASSERT_NE(player, nullptr);

    // Should not crash
    player->EnableVideoFrameCallback(true);
    player->EnableVideoFrameCallback(false);
}

// Test 4: Verify EnableAudioFrameCallback works
TEST(PlayerBypassTest, EnableAudioFrameCallback) {
    auto player = CreatePlayer();
    ASSERT_NE(player, nullptr);

    // Should not crash
    player->EnableAudioFrameCallback(true);
    player->EnableAudioFrameCallback(false);
}

// Test 5: Verify SetCallbackVideoFormat works
TEST(PlayerBypassTest, SetCallbackVideoFormat) {
    auto player = CreatePlayer();
    ASSERT_NE(player, nullptr);

    // Should not crash
    player->SetCallbackVideoFormat(0);  // AV_PIX_FMT_YUV420P
    player->SetCallbackVideoFormat(3);  // AV_PIX_FMT_BGR24
}

// Test 6: Verify SetCallbackAudioFormat works
TEST(PlayerBypassTest, SetCallbackAudioFormat) {
    auto player = CreatePlayer();
    ASSERT_NE(player, nullptr);

    // Should not crash
    player->SetCallbackAudioFormat(1);  // AV_SAMPLE_FMT_S16
    player->SetCallbackAudioFormat(3);  // AV_SAMPLE_FMT_FLT
}

// Test 7: Verify SetDataBypass with null returns error
TEST(PlayerBypassTest, SetDataBypassNull) {
    auto player = CreatePlayer();
    ASSERT_NE(player, nullptr);

    auto result = player->SetDataBypass(nullptr);
    EXPECT_EQ(result, ErrorCode::InvalidParameter);
}
