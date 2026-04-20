#include <gtest/gtest.h>
#include "avsdk/filter.h"
#include "avsdk/types.h"

using namespace avsdk;

// Test 1: Verify interface can be implemented
TEST(FilterProcessorTest, InterfaceExists) {
    // Test implementation of IFilterProcessor
    class TestFilterProcessor : public IFilterProcessor {
    public:
        bool initialized = false;

        int Initialize(const std::string& videoFilterDesc,
                       const std::string& audioFilterDesc = "") override {
            initialized = !videoFilterDesc.empty();
            return 0;
        }

        int ProcessVideoFrame(VideoFrame& frame) override {
            return initialized ? 0 : -1;
        }

        int ProcessAudioFrame(AudioFrame& frame) override {
            return initialized ? 0 : -1;
        }

        bool IsInitialized() const override {
            return initialized;
        }

        void Release() override {
            initialized = false;
        }
    };

    auto filter = std::make_shared<TestFilterProcessor>();
    EXPECT_NE(filter, nullptr);
    EXPECT_FALSE(filter->IsInitialized());

    // Initialize with a simple filter
    int result = filter->Initialize("format=gray");
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(filter->IsInitialized());

    // Create a test video frame
    VideoFrame vFrame{};
    vFrame.data[0] = nullptr;
    vFrame.linesize[0] = 0;
    vFrame.resolution = VideoResolution{1920, 1080};
    vFrame.format = 0;
    vFrame.pts = 0;

    // Process should succeed when initialized
    result = filter->ProcessVideoFrame(vFrame);
    EXPECT_EQ(result, 0);

    // Release
    filter->Release();
    EXPECT_FALSE(filter->IsInitialized());
}

// Test 2: Verify CreateFilterProcessor works
TEST(FilterProcessorTest, CreateFilter) {
    auto filter = CreateFilterProcessor();
    EXPECT_NE(filter, nullptr);
    EXPECT_FALSE(filter->IsInitialized());
}

// Test 3: Test initialize and process with scale filter
TEST(FilterProcessorTest, InitializeAndProcess) {
    auto filter = CreateFilterProcessor();
    ASSERT_NE(filter, nullptr);

    // Initialize with scale filter
    int result = filter->Initialize(Filters::Scale(640, 480));
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(filter->IsInitialized());

    // Create a test video frame with YUV420P format
    VideoFrame vFrame{};
    vFrame.resolution = VideoResolution{1920, 1080};
    vFrame.format = 0;  // AV_PIX_FMT_YUV420P
    vFrame.pts = 1000;

    // Note: We can't actually test processing without real frame data
    // The test validates the interface contract

    filter->Release();
    EXPECT_FALSE(filter->IsInitialized());
}

// Test 4: Test common filter presets
TEST(FilterProcessorTest, CommonFilters) {
    auto filter = CreateFilterProcessor();
    ASSERT_NE(filter, nullptr);

    // Test grayscale filter
    int result = filter->Initialize(Filters::Grayscale);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(filter->IsInitialized());
    filter->Release();

    // Test horizontal flip filter
    result = filter->Initialize(Filters::HorizontalFlip);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(filter->IsInitialized());
    filter->Release();

    // Test invert filter
    result = filter->Initialize(Filters::Invert);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(filter->IsInitialized());
    filter->Release();

    // Test scale filter
    result = filter->Initialize(Filters::Scale(1280, 720));
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(filter->IsInitialized());
    filter->Release();

    // Test crop filter
    result = filter->Initialize(Filters::Crop(640, 480, 100, 100));
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(filter->IsInitialized());
    filter->Release();
}

// Test 5: Test filter preset strings are correct
TEST(FilterProcessorTest, FilterPresetStrings) {
    EXPECT_EQ(Filters::Grayscale, std::string("format=gray"));
    EXPECT_EQ(Filters::Invert, std::string("negate"));
    EXPECT_EQ(Filters::HorizontalFlip, std::string("hflip"));
    EXPECT_EQ(Filters::VerticalFlip, std::string("vflip"));

    // Test Scale returns correct format
    std::string scaleFilter = Filters::Scale(1920, 1080);
    EXPECT_EQ(scaleFilter, std::string("scale=1920:1080"));

    // Test Crop returns correct format
    std::string cropFilter = Filters::Crop(640, 480, 100, 200);
    EXPECT_EQ(cropFilter, std::string("crop=640:480:100:200"));

    // Test Adjust returns correct format (std::to_string may vary precision)
    std::string adjustFilter = Filters::Adjust(0.1f, 1.2f, 1.5f);
    // The format should contain the expected components
    EXPECT_NE(adjustFilter.find("eq="), std::string::npos);
    EXPECT_NE(adjustFilter.find("brightness="), std::string::npos);
    EXPECT_NE(adjustFilter.find("contrast="), std::string::npos);
    EXPECT_NE(adjustFilter.find("saturation="), std::string::npos);
}

// Test 6: Test audio filter presets
TEST(FilterProcessorTest, AudioFilterPresets) {
    EXPECT_EQ(Filters::VolumeUp, std::string("volume=2.0"));
    EXPECT_EQ(Filters::VolumeDown, std::string("volume=0.5"));
    EXPECT_EQ(Filters::Mono, std::string("pan=mono|c0=0.5*c0+0.5*c1"));
}

// Test 7: Test rotate filters
TEST(FilterProcessorTest, RotateFilters) {
    EXPECT_EQ(Filters::Rotate90, std::string("transpose=1"));
    EXPECT_EQ(Filters::Rotate180, std::string("transpose=2,transpose=2"));
    EXPECT_EQ(Filters::Rotate270, std::string("transpose=2"));
}

// Test 8: Test sepia filter
TEST(FilterProcessorTest, SepiaFilter) {
    EXPECT_EQ(Filters::Sepia, std::string("colorchannelmixer=.393:.769:.189:0:.349:.686:.168:0:.272:.534:.131"));
}
