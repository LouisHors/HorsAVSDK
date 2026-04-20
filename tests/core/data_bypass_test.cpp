#include <gtest/gtest.h>
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

// Test 1: Verify interface can be implemented
TEST(DataBypassTest, InterfaceExists) {
    auto bypass = std::make_shared<TestDataBypass>();
    EXPECT_NE(bypass, nullptr);

    // Create test data
    EncodedPacket packet{};
    packet.data = nullptr;
    packet.size = 0;
    packet.pts = 0;
    packet.dts = 0;
    packet.keyFrame = false;
    packet.streamIndex = 0;

    VideoFrame vFrame{};
    vFrame.data[0] = nullptr;
    vFrame.linesize[0] = 0;
    vFrame.resolution = VideoResolution{1920, 1080};
    vFrame.format = 0;
    vFrame.pts = 0;

    AudioFrame aFrame{};
    aFrame.data = nullptr;
    aFrame.samples = 0;
    aFrame.format = 0;
    aFrame.sampleRate = 44100;
    aFrame.channels = 2;
    aFrame.pts = 0;

    // Call all interface methods
    bypass->OnRawPacket(packet);
    bypass->OnDecodedVideoFrame(vFrame);
    bypass->OnDecodedAudioFrame(aFrame);
    bypass->OnPreRenderVideoFrame(vFrame);
    bypass->OnPreRenderAudioFrame(aFrame);
    bypass->OnEncodedPacket(packet);

    EXPECT_EQ(bypass->rawPacketCount, 1);
    EXPECT_EQ(bypass->decodedVideoCount, 1);
    EXPECT_EQ(bypass->decodedAudioCount, 1);
    EXPECT_EQ(bypass->preRenderVideoCount, 1);
    EXPECT_EQ(bypass->preRenderAudioCount, 1);
    EXPECT_EQ(bypass->encodedPacketCount, 1);
}

// Test 2: Verify manager can be created
TEST(DataBypassTest, DataBypassManagerExists) {
    DataBypassManager manager;
    EXPECT_EQ(manager.GetBypassCount(), 0);
}

// Test 3: Verify RegisterBypass works
TEST(DataBypassTest, RegisterBypass) {
    DataBypassManager manager;
    auto bypass = std::make_shared<TestDataBypass>();

    manager.RegisterBypass(bypass);
    EXPECT_EQ(manager.GetBypassCount(), 1);

    // Create test data
    EncodedPacket packet{};
    packet.data = nullptr;
    packet.size = 0;
    packet.pts = 1000;
    packet.dts = 1000;
    packet.keyFrame = true;
    packet.streamIndex = 0;

    VideoFrame vFrame{};
    vFrame.data[0] = nullptr;
    vFrame.linesize[0] = 0;
    vFrame.resolution = VideoResolution{1920, 1080};
    vFrame.format = 0;
    vFrame.pts = 1000;

    AudioFrame aFrame{};
    aFrame.data = nullptr;
    aFrame.samples = 1024;
    aFrame.format = 0;
    aFrame.sampleRate = 44100;
    aFrame.channels = 2;
    aFrame.pts = 1000;

    // Dispatch to all callbacks
    manager.DispatchRawPacket(packet);
    manager.DispatchDecodedVideoFrame(vFrame);
    manager.DispatchDecodedAudioFrame(aFrame);
    manager.DispatchPreRenderVideoFrame(vFrame);
    manager.DispatchPreRenderAudioFrame(aFrame);
    manager.DispatchEncodedPacket(packet);

    EXPECT_EQ(bypass->rawPacketCount, 1);
    EXPECT_EQ(bypass->decodedVideoCount, 1);
    EXPECT_EQ(bypass->decodedAudioCount, 1);
    EXPECT_EQ(bypass->preRenderVideoCount, 1);
    EXPECT_EQ(bypass->preRenderAudioCount, 1);
    EXPECT_EQ(bypass->encodedPacketCount, 1);
}
