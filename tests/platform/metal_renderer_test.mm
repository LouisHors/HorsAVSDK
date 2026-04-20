#include <gtest/gtest.h>
#include "platform/macos/metal_renderer.h"
#include <Cocoa/Cocoa.h>

using namespace avsdk;

TEST(MetalRendererTest, CreateInstance) {
    auto renderer = CreateMetalRenderer();
    EXPECT_NE(renderer, nullptr);
}

TEST(MetalRendererTest, InitializeWithWindow) {
    // Create a test window
    NSWindow* window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 640, 480)
        styleMask:NSWindowStyleMaskTitled
        backing:NSBackingStoreBuffered
        defer:NO];

    auto renderer = CreateMetalRenderer();
    auto result = renderer->Initialize((__bridge void*)window.contentView, 640, 480);
    EXPECT_EQ(result, ErrorCode::OK);

    [window close];
}
