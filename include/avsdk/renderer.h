#pragma once
#include <memory>
#include "avsdk/error.h"

struct AVFrame;

namespace avsdk {

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual ErrorCode Initialize(void* native_window, int width, int height) = 0;
    virtual ErrorCode RenderFrame(const AVFrame* frame) = 0;
    virtual void Release() = 0;
};

} // namespace avsdk
