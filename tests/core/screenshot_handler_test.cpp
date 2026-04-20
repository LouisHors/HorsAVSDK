#include <gtest/gtest.h>
#include "avsdk/screenshot.h"
#include "avsdk/types.h"

using namespace avsdk;

// Test implementation of IScreenshotHandler
class TestScreenshotHandler : public IScreenshotHandler {
public:
    bool captureCalled = false;
    bool saveCalled = false;
    bool getDataCalled = false;
    bool getPNGCalled = false;
    mutable bool hasDataCalled = false;
    bool clearCalled = false;

    int Capture(const VideoFrame& frame) override {
        captureCalled = true;
        return 0;
    }

    int Save(const std::string& path) override {
        saveCalled = true;
        return 0;
    }

    int GetData(std::vector<uint8_t>& data, int& width, int& height) override {
        getDataCalled = true;
        width = 1920;
        height = 1080;
        data.resize(1920 * 1080 * 3);
        return 0;
    }

    int GetPNG(std::vector<uint8_t>& data) override {
        getPNGCalled = true;
        return 0;
    }

    bool HasData() const override {
        hasDataCalled = true;
        return true;
    }

    void Clear() override {
        clearCalled = true;
    }
};

// Test 1: Verify interface can be implemented
TEST(ScreenshotHandlerTest, InterfaceExists) {
    auto handler = std::make_shared<TestScreenshotHandler>();
    EXPECT_NE(handler, nullptr);

    // Create test video frame (YUV420P format)
    VideoFrame frame{};
    frame.resolution = VideoResolution{320, 240};
    frame.format = 0;  // AV_PIX_FMT_YUV420P
    frame.pts = 1000;

    // Call all interface methods
    EXPECT_EQ(handler->Capture(frame), 0);
    EXPECT_EQ(handler->Save("test.png"), 0);

    std::vector<uint8_t> data;
    int width, height;
    EXPECT_EQ(handler->GetData(data, width, height), 0);
    EXPECT_EQ(width, 1920);
    EXPECT_EQ(height, 1080);
    EXPECT_FALSE(data.empty());

    std::vector<uint8_t> pngData;
    EXPECT_EQ(handler->GetPNG(pngData), 0);

    EXPECT_TRUE(handler->HasData());
    handler->Clear();

    EXPECT_TRUE(handler->captureCalled);
    EXPECT_TRUE(handler->saveCalled);
    EXPECT_TRUE(handler->getDataCalled);
    EXPECT_TRUE(handler->getPNGCalled);
    EXPECT_TRUE(handler->hasDataCalled);
    EXPECT_TRUE(handler->clearCalled);
}

// Test 2: Verify CreateScreenshotHandler works
TEST(ScreenshotHandlerTest, CreateScreenshotBypass) {
    auto handler = CreateScreenshotHandler();
    EXPECT_NE(handler, nullptr);
}

// Test 3: Test capture and get data
TEST(ScreenshotHandlerTest, CaptureAndGetData) {
    auto handler = CreateScreenshotHandler();
    EXPECT_NE(handler, nullptr);

    // Create a simple VideoFrame with YUV420P format
    // YUV420P: 320x240, Y plane is 320x240, U and V planes are 160x120
    VideoFrame frame{};
    frame.resolution = VideoResolution{320, 240};
    frame.format = 0;  // AV_PIX_FMT_YUV420P = 0
    frame.pts = 1000;

    // Allocate YUV data
    std::vector<uint8_t> yData(320 * 240);
    std::vector<uint8_t> uData(160 * 120);
    std::vector<uint8_t> vData(160 * 120);

    // Fill with test pattern
    std::fill(yData.begin(), yData.end(), 128);
    std::fill(uData.begin(), uData.end(), 128);
    std::fill(vData.begin(), vData.end(), 128);

    frame.data[0] = yData.data();
    frame.data[1] = uData.data();
    frame.data[2] = vData.data();
    frame.data[3] = nullptr;
    frame.linesize[0] = 320;
    frame.linesize[1] = 160;
    frame.linesize[2] = 160;
    frame.linesize[3] = 0;

    // Capture the frame
    int result = handler->Capture(frame);
    EXPECT_EQ(result, 0);

    // Verify HasData returns true after capture
    EXPECT_TRUE(handler->HasData());

    // Get the data
    std::vector<uint8_t> data;
    int width, height;
    result = handler->GetData(data, width, height);
    EXPECT_EQ(result, 0);

    // Verify dimensions
    EXPECT_EQ(width, 320);
    EXPECT_EQ(height, 240);

    // RGB24 data should be width * height * 3
    EXPECT_EQ(data.size(), 320 * 240 * 3);

    // Clear and verify
    handler->Clear();
    EXPECT_FALSE(handler->HasData());
}
