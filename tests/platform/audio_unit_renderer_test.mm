#include <gtest/gtest.h>
#include "platform/macos/audio_unit_renderer.h"
#include "avsdk/error.h"

using namespace avsdk;

TEST(AudioUnitRendererTest, CreateInstance) {
    auto renderer = CreateAudioUnitRenderer();
    EXPECT_NE(renderer, nullptr);
}

TEST(AudioUnitRendererTest, Initialize) {
    auto renderer = CreateAudioUnitRenderer();

    AudioFormat format;
    format.sample_rate = 48000;
    format.channels = 2;
    format.bits_per_sample = 16;

    auto result = renderer->Initialize(format);
    EXPECT_EQ(result, ErrorCode::OK);
}

TEST(AudioUnitRendererTest, PlayPauseStop) {
    auto renderer = CreateAudioUnitRenderer();

    AudioFormat format;
    format.sample_rate = 48000;
    format.channels = 2;

    renderer->Initialize(format);

    EXPECT_EQ(renderer->GetState(), RendererState::kStopped);

    renderer->Play();
    EXPECT_EQ(renderer->GetState(), RendererState::kPlaying);

    renderer->Pause();
    EXPECT_EQ(renderer->GetState(), RendererState::kPaused);

    renderer->Stop();
    EXPECT_EQ(renderer->GetState(), RendererState::kStopped);
}
