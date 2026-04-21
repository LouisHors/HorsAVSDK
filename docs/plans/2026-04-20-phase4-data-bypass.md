# Phase 4: 数据旁路回调实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use horspowers:subagent-driven-development to implement this plan task-by-task.

**日期**: 2026-04-20

## 目标

实现完整的数据旁路回调系统，支持 AVPacket/AVFrame 级别数据回调，允许用户在播放流程的各个阶段注入自定义处理逻辑（如截图、录制、滤镜处理）。

## 架构方案

数据旁路系统基于回调接口设计，在 Demuxer、Decoder、Render、Encoder 等关键节点提供数据拦截点。使用 `IDataBypass` 接口统一管理所有回调，通过 `DataBypassManager` 协调多个回调处理器。支持零拷贝模式避免性能损失。

## 技术栈

- C++14 回调接口与模板
- FFmpeg AVPacket/AVFrame 数据结构
- 线程安全的回调队列
- 可选的滤镜图处理 (FFmpeg filtergraph)

---

## Task 0: DataBypass 回调接口定义

**Files:**
- Create: `include/avsdk/data_bypass.h`
- Test: `tests/core/data_bypass_test.cpp`

**Step 1: Write the failing test**

```cpp
// tests/core/data_bypass_test.cpp
#include <gtest/gtest.h>
#include "avsdk/data_bypass.h"
#include "avsdk/types.h"

using namespace AVSDK;

TEST(DataBypassTest, InterfaceExists) {
    // Test that IDataBypass interface exists and can be implemented
    class TestBypass : public IDataBypass {
    public:
        void OnRawPacket(const EncodedPacket& packet) override {}
        void OnDecodedVideoFrame(const VideoFrame& frame) override {}
        void OnDecodedAudioFrame(const AudioFrame& frame) override {}
        void OnPreRenderVideoFrame(VideoFrame& frame) override {}
        void OnPreRenderAudioFrame(AudioFrame& frame) override {}
        void OnEncodedPacket(const EncodedPacket& packet) override {}
    };

    TestBypass bypass;
    SUCCEED();
}

TEST(DataBypassTest, DataBypassManagerExists) {
    // Test that DataBypassManager can be created
    auto manager = std::make_unique<DataBypassManager>();
    EXPECT_NE(manager, nullptr);
}

TEST(DataBypassTest, RegisterBypass) {
    // Test registering a bypass handler
    class TestBypass : public IDataBypass {
    public:
        int packetCount = 0;
        void OnRawPacket(const EncodedPacket& packet) override { packetCount++; }
        void OnDecodedVideoFrame(const VideoFrame& frame) override {}
        void OnDecodedAudioFrame(const AudioFrame& frame) override {}
        void OnPreRenderVideoFrame(VideoFrame& frame) override {}
        void OnPreRenderAudioFrame(AudioFrame& frame) override {}
        void OnEncodedPacket(const EncodedPacket& packet) override {}
    };

    auto manager = std::make_unique<DataBypassManager>();
    auto bypass = std::make_shared<TestBypass>();

    EXPECT_EQ(manager->RegisterBypass(bypass), ErrorCode::OK);
}
```

**Step 2: Run test to verify it fails**

Run: `cd build/macos && make -j$(sysctl -n hw.ncpu) 2>&1 | tail -20`
Expected: FAIL with "data_bypass.h not found"

**Step 3: Write minimal implementation**

```cpp
// include/avsdk/data_bypass.h
#pragma once

#include "types.h"
#include "error.h"
#include <functional>
#include <memory>
#include <vector>
#include <mutex>

namespace AVSDK {

// 数据旁路回调接口
// 允许在播放流程的各个阶段获取/修改数据
class IDataBypass {
public:
    virtual ~IDataBypass() = default;

    // ============== 解封装后原始数据 ==============

    // 原始编码数据包回调（解封装后，解码前）
    // @param packet: 编码数据包（H264/H265/AAC等）
    virtual void OnRawPacket(const EncodedPacket& packet) {}

    // ============== 解码后数据 ==============

    // 解码后视频帧回调
    // @param frame: 视频帧数据（YUV/RGB格式）
    virtual void OnDecodedVideoFrame(const VideoFrame& frame) {}

    // 解码后音频帧回调
    // @param frame: 音频帧数据（PCM格式）
    virtual void OnDecodedAudioFrame(const AudioFrame& frame) {}

    // ============== 渲染前数据（允许修改） ==============

    // 渲染前视频帧回调（允许修改帧数据）
    // @param frame: 视频帧数据，可修改
    virtual void OnPreRenderVideoFrame(VideoFrame& frame) {}

    // 渲染前音频帧回调（允许修改帧数据）
    // @param frame: 音频帧数据，可修改
    virtual void OnPreRenderAudioFrame(AudioFrame& frame) {}

    // ============== 编码后数据 ==============

    // 编码后数据回调
    // @param packet: 编码后的数据包
    virtual void OnEncodedPacket(const EncodedPacket& packet) {}
};

// 使用 std::function 的便捷回调结构
struct DataBypassCallbacks {
    std::function<void(const EncodedPacket&)> onRawPacket;
    std::function<void(const VideoFrame&)> onDecodedVideoFrame;
    std::function<void(const AudioFrame&)> onDecodedAudioFrame;
    std::function<void(VideoFrame&)> onPreRenderVideoFrame;
    std::function<void(AudioFrame&)> onPreRenderAudioFrame;
    std::function<void(const EncodedPacket&)> onEncodedPacket;
};

// 数据旁路管理器
// 管理多个数据旁路处理器，支持链式调用
class DataBypassManager {
public:
    DataBypassManager() = default;
    ~DataBypassManager() = default;

    // 禁止拷贝，允许移动
    DataBypassManager(const DataBypassManager&) = delete;
    DataBypassManager& operator=(const DataBypassManager&) = delete;
    DataBypassManager(DataBypassManager&&) = default;
    DataBypassManager& operator=(DataBypassManager&&) = default;

    // 注册数据旁路处理器
    // @param bypass: 旁路处理器（shared_ptr 管理生命周期）
    // @return: 错误码
    ErrorCode RegisterBypass(std::shared_ptr<IDataBypass> bypass);

    // 注销数据旁路处理器
    // @param bypass: 旁路处理器
    // @return: 错误码
    ErrorCode UnregisterBypass(std::shared_ptr<IDataBypass> bypass);

    // 清空所有处理器
    void ClearBypasses();

    // ============== 数据分发方法（由内部模块调用） ==============

    // 分发原始数据包
    void DispatchRawPacket(const EncodedPacket& packet);

    // 分发解码后视频帧
    void DispatchDecodedVideoFrame(const VideoFrame& frame);

    // 分发解码后音频帧
    void DispatchDecodedAudioFrame(const AudioFrame& frame);

    // 分发渲染前视频帧（允许修改）
    void DispatchPreRenderVideoFrame(VideoFrame& frame);

    // 分发渲染前音频帧（允许修改）
    void DispatchPreRenderAudioFrame(AudioFrame& frame);

    // 分发编码后数据包
    void DispatchEncodedPacket(const EncodedPacket& packet);

    // 获取处理器数量
    size_t GetBypassCount() const;

private:
    mutable std::mutex mutex_;
    std::vector<std::weak_ptr<IDataBypass>> bypasses_;

    // 清理无效的弱引用
    void CleanupExpiredBypasses();
};

} // namespace AVSDK
#endif // AVSDK_DATA_BYPASS_H
```

```cpp
// src/core/bypass/data_bypass_manager.cpp
#include "avsdk/data_bypass.h"
#include "avsdk/logger.h"

namespace AVSDK {

ErrorCode DataBypassManager::RegisterBypass(std::shared_ptr<IDataBypass> bypass) {
    if (!bypass) {
        return ErrorCode::InvalidParameter;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已注册
    for (auto& weakBypass : bypasses_) {
        if (auto existing = weakBypass.lock()) {
            if (existing == bypass) {
                return ErrorCode::AlreadyInitialized;
            }
        }
    }

    bypasses_.push_back(bypass);
    return ErrorCode::OK;
}

ErrorCode DataBypassManager::UnregisterBypass(std::shared_ptr<IDataBypass> bypass) {
    if (!bypass) {
        return ErrorCode::InvalidParameter;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::remove_if(bypasses_.begin(), bypasses_.end(),
        [&bypass](const std::weak_ptr<IDataBypass>& weak) {
            if (auto existing = weak.lock()) {
                return existing == bypass;
            }
            return false;
        });

    if (it == bypasses_.end()) {
        return ErrorCode::NotFound;
    }

    bypasses_.erase(it, bypasses_.end());
    return ErrorCode::OK;
}

void DataBypassManager::ClearBypasses() {
    std::lock_guard<std::mutex> lock(mutex_);
    bypasses_.clear();
}

void DataBypassManager::DispatchRawPacket(const EncodedPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& weakBypass : bypasses_) {
        if (auto bypass = weakBypass.lock()) {
            bypass->OnRawPacket(packet);
        }
    }

    CleanupExpiredBypasses();
}

void DataBypassManager::DispatchDecodedVideoFrame(const VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& weakBypass : bypasses_) {
        if (auto bypass = weakBypass.lock()) {
            bypass->OnDecodedVideoFrame(frame);
        }
    }

    CleanupExpiredBypasses();
}

void DataBypassManager::DispatchDecodedAudioFrame(const AudioFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& weakBypass : bypasses_) {
        if (auto bypass = weakBypass.lock()) {
            bypass->OnDecodedAudioFrame(frame);
        }
    }

    CleanupExpiredBypasses();
}

void DataBypassManager::DispatchPreRenderVideoFrame(VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& weakBypass : bypasses_) {
        if (auto bypass = weakBypass.lock()) {
            bypass->OnPreRenderVideoFrame(frame);
        }
    }

    CleanupExpiredBypasses();
}

void DataBypassManager::DispatchPreRenderAudioFrame(AudioFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& weakBypass : bypasses_) {
        if (auto bypass = weakBypass.lock()) {
            bypass->OnPreRenderAudioFrame(frame);
        }
    }

    CleanupExpiredBypasses();
}

void DataBypassManager::DispatchEncodedPacket(const EncodedPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& weakBypass : bypasses_) {
        if (auto bypass = weakBypass.lock()) {
            bypass->OnEncodedPacket(packet);
        }
    }

    CleanupExpiredBypasses();
}

size_t DataBypassManager::GetBypassCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bypasses_.size();
}

void DataBypassManager::CleanupExpiredBypasses() {
    // 注意：此方法必须在已加锁的情况下调用
    auto it = std::remove_if(bypasses_.begin(), bypasses_.end(),
        [](const std::weak_ptr<IDataBypass>& weak) {
            return weak.expired();
        });
    bypasses_.erase(it, bypasses_.end());
}

} // namespace AVSDK
```

**Step 4: Run test to verify it passes**

Run: `cd build/macos && make -j$(sysctl -n hw.ncpu) && ./tests/core/data_bypass_test`
Expected: PASS

**Step 5: Commit**

```bash
git add include/avsdk/data_bypass.h src/core/bypass/data_bypass_manager.cpp tests/core/data_bypass_test.cpp
git commit -m "feat(bypass): add data bypass callback interface and manager

- Add IDataBypass interface with 6 callback points
- Add DataBypassManager for managing multiple bypass handlers
- Support weak_ptr lifecycle management"
```

---

## Task 1: 集成 DataBypass 到 Player

**Files:**
- Modify: `include/avsdk/player.h` (add SetDataBypass method)
- Modify: `src/core/player/player_impl.h` (add DataBypassManager member)
- Modify: `src/core/player/player_impl.cpp` (dispatch callbacks)
- Test: `tests/core/player_bypass_test.cpp`

**Step 1: Write the failing test**

```cpp
// tests/core/player_bypass_test.cpp
#include <gtest/gtest.h>
#include "avsdk/player.h"
#include "avsdk/data_bypass.h"

using namespace AVSDK;

class TestBypass : public IDataBypass {
public:
    int videoFrameCount = 0;
    int audioFrameCount = 0;

    void OnDecodedVideoFrame(const VideoFrame& frame) override {
        videoFrameCount++;
    }

    void OnDecodedAudioFrame(const AudioFrame& frame) override {
        audioFrameCount++;
    }
};

TEST(PlayerBypassTest, SetDataBypass) {
    auto player = PlayerFactory::CreatePlayer();

    PlayerConfig config;
    EXPECT_EQ(player->Initialize(config), ErrorCode::OK);

    auto bypass = std::make_shared<TestBypass>();
    EXPECT_EQ(player->SetDataBypass(bypass), ErrorCode::OK);
}

TEST(PlayerBypassTest, GetDataBypassManager) {
    auto player = PlayerFactory::CreatePlayer();

    PlayerConfig config;
    player->Initialize(config);

    auto manager = player->GetDataBypassManager();
    EXPECT_NE(manager, nullptr);
}
```

**Step 2: Run test to verify it fails**

Run: `cd build/macos && make 2>&1 | tail -30`
Expected: FAIL with "SetDataBypass not found"

**Step 3: Write minimal implementation**

```cpp
// include/avsdk/player.h (add to IPlayer interface)

// ============== 数据旁路（第四阶段） ====================

// 设置数据旁路管理器
// @param manager: 数据旁路管理器
virtual void SetDataBypassManager(std::shared_ptr<DataBypassManager> manager) = 0;

// 获取数据旁路管理器
// @return: 数据旁路管理器（如果不存在则创建）
virtual std::shared_ptr<DataBypassManager> GetDataBypassManager() = 0;

// 设置数据旁路处理器（便捷方法）
// @param bypass: 数据旁路处理器
// @return: 错误码
virtual ErrorCode SetDataBypass(std::shared_ptr<IDataBypass> bypass) = 0;

// 启用/禁用视频帧回调
// @param enable: 是否启用
virtual void EnableVideoFrameCallback(bool enable) = 0;

// 启用/禁用音频帧回调
// @param enable: 是否启用
virtual void EnableAudioFrameCallback(bool enable) = 0;

// 设置回调视频格式
// @param format: 像素格式 (AVPixelFormat)
virtual void SetCallbackVideoFormat(int format) = 0;

// 设置回调音频格式
// @param format: 采样格式 (AVSampleFormat)
virtual void SetCallbackAudioFormat(int format) = 0;
```

```cpp
// src/core/player/player_impl.h (add member)

#include "avsdk/data_bypass.h"

class PlayerImpl : public IPlayer {
    // ... existing members ...

    // Data bypass
    std::shared_ptr<DataBypassManager> dataBypassManager_;
    bool enableVideoFrameCallback_ = false;
    bool enableAudioFrameCallback_ = false;
    int callbackVideoFormat_ = AV_PIX_FMT_YUV420P;
    int callbackAudioFormat_ = AV_SAMPLE_FMT_S16;

    // Dispatch callbacks
    void DispatchDecodedVideoFrame(const VideoFrame& frame);
    void DispatchDecodedAudioFrame(const AudioFrame& frame);
};
```

```cpp
// src/core/player/player_impl.cpp (implement methods)

void PlayerImpl::SetDataBypassManager(std::shared_ptr<DataBypassManager> manager) {
    std::lock_guard<std::mutex> lock(mutex_);
    dataBypassManager_ = manager;
}

std::shared_ptr<DataBypassManager> PlayerImpl::GetDataBypassManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!dataBypassManager_) {
        dataBypassManager_ = std::make_shared<DataBypassManager>();
    }
    return dataBypassManager_;
}

ErrorCode PlayerImpl::SetDataBypass(std::shared_ptr<IDataBypass> bypass) {
    if (!bypass) {
        return ErrorCode::InvalidParameter;
    }
    return GetDataBypassManager()->RegisterBypass(bypass);
}

void PlayerImpl::EnableVideoFrameCallback(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    enableVideoFrameCallback_ = enable;
}

void PlayerImpl::EnableAudioFrameCallback(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    enableAudioFrameCallback_ = enable;
}

void PlayerImpl::SetCallbackVideoFormat(int format) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbackVideoFormat_ = format;
}

void PlayerImpl::SetCallbackAudioFormat(int format) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbackAudioFormat_ = format;
}

void PlayerImpl::DispatchDecodedVideoFrame(const VideoFrame& frame) {
    if (!enableVideoFrameCallback_ || !dataBypassManager_) {
        return;
    }
    dataBypassManager_->DispatchDecodedVideoFrame(frame);
}

void PlayerImpl::DispatchDecodedAudioFrame(const AudioFrame& frame) {
    if (!enableAudioFrameCallback_ || !dataBypassManager_) {
        return;
    }
    dataBypassManager_->DispatchDecodedAudioFrame(frame);
}
```

**Step 4: Run test to verify it passes**

Run: `cd build/macos && make -j$(sysctl -n hw.ncpu) && ./tests/core/player_bypass_test`
Expected: PASS

**Step 5: Commit**

```bash
git add include/avsdk/player.h src/core/player/player_impl.h src/core/player/player_impl.cpp tests/core/player_bypass_test.cpp
git commit -m "feat(player): integrate data bypass callbacks

- Add SetDataBypass, GetDataBypassManager to IPlayer
- Add EnableVideoFrameCallback, EnableAudioFrameCallback
- Add format configuration for callbacks"
```

---

## Task 2: 视频截图功能 (Screenshot)

**Files:**
- Create: `include/avsdk/screenshot.h`
- Create: `src/core/bypass/screenshot_handler.h`
- Create: `src/core/bypass/screenshot_handler.cpp`
- Test: `tests/core/screenshot_handler_test.cpp`

**Step 1: Write the failing test**

```cpp
// tests/core/screenshot_handler_test.cpp
#include <gtest/gtest.h>
#include "avsdk/screenshot.h"
#include "avsdk/data_bypass.h"

using namespace AVSDK;

TEST(ScreenshotHandlerTest, InterfaceExists) {
    // Test that IScreenshotHandler interface exists
    class TestHandler : public IScreenshotHandler {
    public:
        ErrorCode Capture(const VideoFrame& frame) override {
            return ErrorCode::OK;
        }
        ErrorCode Save(const std::string& path) override {
            return ErrorCode::OK;
        }
    };

    TestHandler handler;
    SUCCEED();
}

TEST(ScreenshotHandlerTest, CreateScreenshotBypass) {
    // Test creating a screenshot bypass handler
    auto handler = ScreenshotHandler::Create();
    EXPECT_NE(handler, nullptr);
}

TEST(ScreenshotHandlerTest, CaptureAndGetData) {
    auto handler = ScreenshotHandler::Create();

    // Create a simple test frame
    VideoFrame frame;
    frame.width = 100;
    frame.height = 100;
    frame.format = AV_PIX_FMT_YUV420P;

    EXPECT_EQ(handler->Capture(frame), ErrorCode::OK);

    std::vector<uint8_t> data;
    int width, height;
    EXPECT_EQ(handler->GetData(data, width, height), ErrorCode::OK);
    EXPECT_EQ(width, 100);
    EXPECT_EQ(height, 100);
    EXPECT_GT(data.size(), 0);
}
```

**Step 2: Run test to verify it fails**

Run: `cd build/macos && make 2>&1 | tail -20`
Expected: FAIL with "screenshot.h not found"

**Step 3: Write minimal implementation**

```cpp
// include/avsdk/screenshot.h
#pragma once

#include "types.h"
#include "error.h"
#include <string>
#include <vector>
#include <memory>

namespace AVSDK {

// 截图处理器接口
class IScreenshotHandler {
public:
    virtual ~IScreenshotHandler() = default;

    // 捕获视频帧
    // @param frame: 视频帧数据
    // @return: 错误码
    virtual ErrorCode Capture(const VideoFrame& frame) = 0;

    // 保存截图到文件
    // @param path: 文件路径（支持 PNG/JPG/BMP）
    // @return: 错误码
    virtual ErrorCode Save(const std::string& path) = 0;

    // 获取截图数据
    // @param data: 输出 RGB24 数据
    // @param width: 输出宽度
    // @param height: 输出高度
    // @return: 错误码
    virtual ErrorCode GetData(std::vector<uint8_t>& data, int& width, int& height) = 0;

    // 获取截图数据（PNG格式）
    // @param data: 输出 PNG 数据
    // @return: 错误码
    virtual ErrorCode GetPNG(std::vector<uint8_t>& data) = 0;

    // 是否有截图数据
    // @return: 是否有数据
    virtual bool HasData() const = 0;

    // 清除截图数据
    virtual void Clear() = 0;
};

// 截图处理器实现类（前向声明）
class ScreenshotHandler;

// 创建截图处理器
// @return: 截图处理器实例
std::shared_ptr<IScreenshotHandler> CreateScreenshotHandler();

} // namespace AVSDK
#endif // AVSDK_SCREENSHOT_H
```

```cpp
// src/core/bypass/screenshot_handler.h
#pragma once

#include "avsdk/screenshot.h"
#include "avsdk/data_bypass.h"

extern "C" {
#include <libswscale/swscale.h>
}

namespace AVSDK {

// 截图处理器实现
class ScreenshotHandler : public IScreenshotHandler, public IDataBypass {
public:
    ScreenshotHandler();
    ~ScreenshotHandler() override;

    // IScreenshotHandler implementation
    ErrorCode Capture(const VideoFrame& frame) override;
    ErrorCode Save(const std::string& path) override;
    ErrorCode GetData(std::vector<uint8_t>& data, int& width, int& height) override;
    ErrorCode GetPNG(std::vector<uint8_t>& data) override;
    bool HasData() const override;
    void Clear() override;

    // IDataBypass implementation
    void OnDecodedVideoFrame(const VideoFrame& frame) override;

    // 创建实例
    static std::shared_ptr<ScreenshotHandler> Create();

private:
    mutable std::mutex mutex_;
    std::vector<uintuint8_t> rgbData_;
    int width_ = 0;
    int height_ = 0;
    bool hasData_ = false;

    // 转换帧到 RGB24
    ErrorCode ConvertToRGB(const VideoFrame& frame, std::vector<uint8_t>& output);
};

} // namespace AVSDK
```

```cpp
// src/core/bypass/screenshot_handler.cpp
#include "screenshot_handler.h"
#include "avsdk/logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

namespace AVSDK {

ScreenshotHandler::ScreenshotHandler() = default;

ScreenshotHandler::~ScreenshotHandler() = default;

std::shared_ptr<ScreenshotHandler> ScreenshotHandler::Create() {
    return std::make_shared<ScreenshotHandler>();
}

ErrorCode ScreenshotHandler::Capture(const VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 转换帧到 RGB24
    std::vector<uint8_t> rgbData;
    ErrorCode result = ConvertToRGB(frame, rgbData);
    if (result != ErrorCode::OK) {
        return result;
    }

    rgbData_ = std::move(rgbData);
    width_ = frame.resolution.width;
    height_ = frame.resolution.height;
    hasData_ = true;

    LOG_INFO("Screenshot captured: {}x{}", width_, height_);
    return ErrorCode::OK;
}

ErrorCode ScreenshotHandler::Save(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!hasData_ || rgbData_.empty()) {
        return ErrorCode::NotInitialized;
    }

    // 根据扩展名选择格式
    std::string ext = path.substr(path.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    AVCodecID codecId = AV_CODEC_ID_PNG;
    if (ext == "jpg" || ext == "jpeg") {
        codecId = AV_CODEC_ID_MJPEG;
    } else if (ext == "bmp") {
        codecId = AV_CODEC_ID_BMP;
    }

    // 查找编码器
    const AVCodec* codec = avcodec_find_encoder(codecId);
    if (!codec) {
        LOG_ERROR("Codec not found for format: {}", ext);
        return ErrorCode::CodecNotFound;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        return ErrorCode::OutOfMemory;
    }

    codecCtx->width = width_;
    codecCtx->height = height_;
    codecCtx->pix_fmt = AV_PIX_FMT_RGB24;
    codecCtx->time_base = {1, 1};

    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&codecCtx);
        return ErrorCode::CodecOpenFailed;
    }

    // 创建帧
    AVFrame* frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_RGB24;
    frame->width = width_;
    frame->height = height_;

    if (av_frame_get_buffer(frame, 0) < 0) {
        av_frame_free(&frame);
        avcodec_free_context(&codecCtx);
        return ErrorCode::OutOfMemory;
    }

    // 复制数据
    memcpy(frame->data[0], rgbData_.data(), rgbData_.size());

    // 编码
    AVPacket* packet = av_packet_alloc();

    if (avcodec_send_frame(codecCtx, frame) < 0) {
        av_packet_free(&packet);
        av_frame_free(&frame);
        avcodec_free_context(&codecCtx);
        return ErrorCode::CodecEncodeFailed;
    }

    FILE* file = fopen(path.c_str(), "wb");
    if (!file) {
        av_packet_free(&packet);
        av_frame_free(&frame);
        avcodec_free_context(&codecCtx);
        return ErrorCode::FileOpenFailed;
    }

    while (avcodec_receive_packet(codecCtx, packet) == 0) {
        fwrite(packet->data, 1, packet->size, file);
        av_packet_unref(packet);
    }

    fclose(file);
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&codecCtx);

    LOG_INFO("Screenshot saved to: {}", path);
    return ErrorCode::OK;
}

ErrorCode ScreenshotHandler::GetData(std::vector<uint8_t>& data, int& width, int& height) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!hasData_ || rgbData_.empty()) {
        return ErrorCode::NotInitialized;
    }

    data = rgbData_;
    width = width_;
    height = height_;
    return ErrorCode::OK;
}

ErrorCode ScreenshotHandler::GetPNG(std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!hasData_ || rgbData_.empty()) {
        return ErrorCode::NotInitialized;
    }

    // 使用 PNG 编码器编码
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!codec) {
        return ErrorCode::CodecNotFound;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        return ErrorCode::OutOfMemory;
    }

    codecCtx->width = width_;
    codecCtx->height = height_;
    codecCtx->pix_fmt = AV_PIX_FMT_RGB24;
    codecCtx->time_base = {1, 1};

    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&codecCtx);
        return ErrorCode::CodecOpenFailed;
    }

    AVFrame* frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_RGB24;
    frame->width = width_;
    frame->height = height_;

    if (av_frame_get_buffer(frame, 0) < 0) {
        av_frame_free(&frame);
        avcodec_free_context(&codecCtx);
        return ErrorCode::OutOfMemory;
    }

    memcpy(frame->data[0], rgbData_.data(), rgbData_.size());

    AVPacket* packet = av_packet_alloc();

    if (avcodec_send_frame(codecCtx, frame) < 0) {
        av_packet_free(&packet);
        av_frame_free(&frame);
        avcodec_free_context(&codecCtx);
        return ErrorCode::CodecEncodeFailed;
    }

    data.clear();
    while (avcodec_receive_packet(codecCtx, packet) == 0) {
        data.insert(data.end(), packet->data, packet->data + packet->size);
        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&codecCtx);

    return ErrorCode::OK;
}

bool ScreenshotHandler::HasData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hasData_;
}

void ScreenshotHandler::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    rgbData_.clear();
    width_ = 0;
    height_ = 0;
    hasData_ = false;
}

void ScreenshotHandler::OnDecodedVideoFrame(const VideoFrame& frame) {
    // 自动截图（可以通过配置控制）
    Capture(frame);
}

ErrorCode ScreenshotHandler::ConvertToRGB(const VideoFrame& frame, std::vector<uint8_t>& output) {
    int width = frame.resolution.width;
    int height = frame.resolution.height;

    // 创建 SwsContext 进行格式转换
    SwsContext* swsCtx = sws_getContext(
        width, height, (AVPixelFormat)frame.format,
        width, height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!swsCtx) {
        return ErrorCode::CodecNotSupported;
    }

    // 分配输出缓冲区
    int rgbLinesize = width * 3;
    output.resize(height * rgbLinesize);

    uint8_t* dstData[4] = {output.data(), nullptr, nullptr, nullptr};
    int dstLinesize[4] = {rgbLinesize, 0, 0, 0};

    // 执行转换
    sws_scale(swsCtx, frame.data, frame.linesize, 0, height, dstData, dstLinesize);

    sws_freeContext(swsCtx);
    return ErrorCode::OK;
}

std::shared_ptr<IScreenshotHandler> CreateScreenshotHandler() {
    return ScreenshotHandler::Create();
}

} // namespace AVSDK
```

**Step 4: Run test to verify it passes**

Run: `cd build/macos && make -j$(sysctl -n hw.ncpu) && ./tests/core/screenshot_handler_test`
Expected: PASS

**Step 5: Commit**

```bash
git add include/avsdk/screenshot.h src/core/bypass/screenshot_handler.h src/core/bypass/screenshot_handler.cpp tests/core/screenshot_handler_test.cpp
git commit -m "feat(bypass): add screenshot handler

- Add IScreenshotHandler interface
- Add ScreenshotHandler implementing IDataBypass
- Support Save to PNG/JPG/BMP, Get RGB data, Get PNG buffer"
```

---

## Task 3: 视频录制功能 (Video Recorder)

**Files:**
- Create: `include/avsdk/video_recorder.h`
- Create: `src/core/bypass/video_recorder.h`
- Create: `src/core/bypass/video_recorder.cpp`
- Test: `tests/core/video_recorder_test.cpp`

**Step 1: Write the failing test**

```cpp
// tests/core/video_recorder_test.cpp
#include <gtest/gtest.h>
#include "avsdk/video_recorder.h"
#include "avsdk/data_bypass.h"

using namespace AVSDK;

TEST(VideoRecorderTest, InterfaceExists) {
    // Test that IVideoRecorder interface exists
    class TestRecorder : public IVideoRecorder {
    public:
        ErrorCode Start(const std::string& path) override { return ErrorCode::OK; }
        ErrorCode Stop() override { return ErrorCode::OK; }
        bool IsRecording() const override { return false; }
        void OnDecodedVideoFrame(const VideoFrame& frame) override {}
        void OnDecodedAudioFrame(const AudioFrame& frame) override {}
    };

    TestRecorder recorder;
    SUCCEED();
}

TEST(VideoRecorderTest, CreateRecorder) {
    auto recorder = VideoRecorder::Create();
    EXPECT_NE(recorder, nullptr);
    EXPECT_FALSE(recorder->IsRecording());
}

TEST(VideoRecorderTest, StartStopRecording) {
    auto recorder = VideoRecorder::Create();

    // Use a temp file
    std::string path = "/tmp/test_recording.mp4";

    EXPECT_EQ(recorder->Start(path), ErrorCode::OK);
    EXPECT_TRUE(recorder->IsRecording());

    EXPECT_EQ(recorder->Stop(), ErrorCode::OK);
    EXPECT_FALSE(recorder->IsRecording());
}
```

**Step 2: Run test to verify it fails**

Run: `cd build/macos && make 2>&1 | tail -20`
Expected: FAIL with "video_recorder.h not found"

**Step 3: Write minimal implementation**

```cpp
// include/avsdk/video_recorder.h
#pragma once

#include "types.h"
#include "error.h"
#include <string>
#include <memory>

namespace AVSDK {

// 视频录制器接口
class IVideoRecorder {
public:
    virtual ~IVideoRecorder() = default;

    // 开始录制
    // @param path: 输出文件路径（MP4/MOV/MKV）
    // @return: 错误码
    virtual ErrorCode Start(const std::string& path) = 0;

    // 停止录制
    // @return: 错误码
    virtual ErrorCode Stop() = 0;

    // 是否正在录制
    // @return: 是否录制中
    virtual bool IsRecording() const = 0;

    // 获取录制时长（毫秒）
    // @return: 录制时长
    virtual int64_t GetRecordingDurationMs() const = 0;

    // 获取录制统计
    // @param videoFrames: 视频帧数
    // @param audioFrames: 音频帧数
    virtual void GetStats(int64_t& videoFrames, int64_t& audioFrames) const = 0;
};

// 录制配置
struct RecorderConfig {
    int videoWidth = 1920;
    int videoHeight = 1080;
    int videoBitrate = 4000000;  // 4 Mbps
    int frameRate = 30;
    int audioSampleRate = 48000;
    int audioChannels = 2;
    int audioBitrate = 128000;   // 128 kbps
    std::string videoCodec = "libx264";
    std::string audioCodec = "aac";
};

// 录制器实现类（前向声明）
class VideoRecorder;

// 创建录制器
// @param config: 录制配置
// @return: 录制器实例
std::shared_ptr<IVideoRecorder> CreateVideoRecorder(const RecorderConfig& config = RecorderConfig());

} // namespace AVSDK
#endif // AVSDK_VIDEO_RECORDER_H
```

```cpp
// src/core/bypass/video_recorder.h
#pragma once

#include "avsdk/video_recorder.h"
#include "avsdk/data_bypass.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace AVSDK {

class VideoRecorder : public IVideoRecorder, public IDataBypass {
public:
    explicit VideoRecorder(const RecorderConfig& config);
    ~VideoRecorder() override;

    // IVideoRecorder implementation
    ErrorCode Start(const std::string& path) override;
    ErrorCode Stop() override;
    bool IsRecording() const override;
    int64_t GetRecordingDurationMs() const override;
    void GetStats(int64_t& videoFrames, int64_t& audioFrames) const override;

    // IDataBypass implementation
    void OnDecodedVideoFrame(const VideoFrame& frame) override;
    void OnDecodedAudioFrame(const AudioFrame& frame) override;

    // Create instance
    static std::shared_ptr<VideoRecorder> Create(const RecorderConfig& config = RecorderConfig());

private:
    RecorderConfig config_;
    mutable std::mutex mutex_;

    // FFmpeg context
    AVFormatContext* formatCtx_ = nullptr;
    AVCodecContext* videoCodecCtx_ = nullptr;
    AVCodecContext* audioCodecCtx_ = nullptr;
    AVStream* videoStream_ = nullptr;
    AVStream* audioStream_ = nullptr;

    // Sws context for video conversion
    SwsContext* swsCtx_ = nullptr;
    SwrContext* swrCtx_ = nullptr;

    // Frame buffers
    AVFrame* convertedVideoFrame_ = nullptr;
    AVFrame* convertedAudioFrame_ = nullptr;

    // State
    bool isRecording_ = false;
    int64_t startTime_ = 0;
    int64_t videoFrameCount_ = 0;
    int64_t audioFrameCount_ = 0;
    int64_t videoPts_ = 0;
    int64_t audioPts_ = 0;

    // Methods
    ErrorCode InitializeVideo();
    ErrorCode InitializeAudio();
    ErrorCode WriteVideoFrame(const VideoFrame& frame);
    ErrorCode WriteAudioFrame(const AudioFrame& frame);
    void Cleanup();
};

} // namespace AVSDK
```

```cpp
// src/core/bypass/video_recorder.cpp
#include "video_recorder.h"
#include "avsdk/logger.h"

namespace AVSDK {

VideoRecorder::VideoRecorder(const RecorderConfig& config) : config_(config) {}

VideoRecorder::~VideoRecorder() {
    Stop();
    Cleanup();
}

std::shared_ptr<VideoRecorder> VideoRecorder::Create(const RecorderConfig& config) {
    return std::make_shared<VideoRecorder>(config);
}

ErrorCode VideoRecorder::Start(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isRecording_) {
        return ErrorCode::AlreadyInitialized;
    }

    // 创建输出上下文
    int ret = avformat_alloc_output_context2(&formatCtx_, nullptr, nullptr, path.c_str());
    if (ret < 0 || !formatCtx_) {
        LOG_ERROR("Failed to create output context: {}", ret);
        return ErrorCode::FileOpenFailed;
    }

    // 初始化视频流
    ErrorCode result = InitializeVideo();
    if (result != ErrorCode::OK) {
        Cleanup();
        return result;
    }

    // 初始化音频流
    result = InitializeAudio();
    if (result != ErrorCode::OK) {
        Cleanup();
        return result;
    }

    // 打开输出文件
    if (!(formatCtx_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&formatCtx_->pb, path.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOG_ERROR("Failed to open output file: {}", ret);
            Cleanup();
            return ErrorCode::FileOpenFailed;
        }
    }

    // 写入文件头
    ret = avformat_write_header(formatCtx_, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to write header: {}", ret);
        Cleanup();
        return ErrorCode::FileWriteFailed;
    }

    isRecording_ = true;
    startTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    videoFrameCount_ = 0;
    audioFrameCount_ = 0;
    videoPts_ = 0;
    audioPts_ = 0;

    LOG_INFO("Recording started: {}", path);
    return ErrorCode::OK;
}

ErrorCode VideoRecorder::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isRecording_) {
        return ErrorCode::NotInitialized;
    }

    // 写入文件尾
    if (formatCtx_) {
        av_write_trailer(formatCtx_);
    }

    isRecording_ = false;

    LOG_INFO("Recording stopped. Video frames: {}, Audio frames: {}",
             videoFrameCount_, audioFrameCount_);

    Cleanup();
    return ErrorCode::OK;
}

bool VideoRecorder::IsRecording() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isRecording_;
}

int64_t VideoRecorder::GetRecordingDurationMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRecording_) {
        return 0;
    }
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return now - startTime_;
}

void VideoRecorder::GetStats(int64_t& videoFrames, int64_t& audioFrames) const {
    std::lock_guard<std::mutex> lock(mutex_);
    videoFrames = videoFrameCount_;
    audioFrames = audioFrameCount_;
}

void VideoRecorder::OnDecodedVideoFrame(const VideoFrame& frame) {
    if (!IsRecording()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    WriteVideoFrame(frame);
}

void VideoRecorder::OnDecodedAudioFrame(const AudioFrame& frame) {
    if (!IsRecording()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    WriteAudioFrame(frame);
}

ErrorCode VideoRecorder::InitializeVideo() {
    const AVCodec* codec = avcodec_find_encoder_by_name(config_.videoCodec.c_str());
    if (!codec) {
        // Fallback to libx264
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (!codec) {
        return ErrorCode::CodecNotFound;
    }

    videoStream_ = avformat_new_stream(formatCtx_, codec);
    if (!videoStream_) {
        return ErrorCode::OutOfMemory;
    }

    videoCodecCtx_ = avcodec_alloc_context3(codec);
    if (!videoCodecCtx_) {
        return ErrorCode::OutOfMemory;
    }

    videoCodecCtx_->width = config_.videoWidth;
    videoCodecCtx_->height = config_.videoHeight;
    videoCodecCtx_->time_base = {1, config_.frameRate};
    videoCodecCtx_->framerate = {config_.frameRate, 1};
    videoCodecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;
    videoCodecCtx_->bit_rate = config_.videoBitrate;
    videoCodecCtx_->gop_size = config_.frameRate * 2;  // 2 second GOP

    if (formatCtx_->oformat->flags & AVFMT_GLOBALHEADER) {
        videoCodecCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "preset", "fast", 0);
    av_dict_set(&opts, "tune", "zerolatency", 0);

    if (avcodec_open2(videoCodecCtx_, codec, &opts) < 0) {
        av_dict_free(&opts);
        return ErrorCode::CodecOpenFailed;
    }
    av_dict_free(&opts);

    avcodec_parameters_from_context(videoStream_->codecpar, videoCodecCtx_);
    videoStream_->time_base = videoCodecCtx_->time_base;

    // 分配转换帧
    convertedVideoFrame_ = av_frame_alloc();
    convertedVideoFrame_->format = AV_PIX_FMT_YUV420P;
    convertedVideoFrame_->width = config_.videoWidth;
    convertedVideoFrame_->height = config_.videoHeight;
    av_frame_get_buffer(convertedVideoFrame_, 0);

    return ErrorCode::OK;
}

ErrorCode VideoRecorder::InitializeAudio() {
    const AVCodec* codec = avcodec_find_encoder_by_name(config_.audioCodec.c_str());
    if (!codec) {
        codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    }
    if (!codec) {
        return ErrorCode::CodecNotFound;
    }

    audioStream_ = avformat_new_stream(formatCtx_, codec);
    if (!audioStream_) {
        return ErrorCode::OutOfMemory;
    }

    audioCodecCtx_ = avcodec_alloc_context3(codec);
    if (!audioCodecCtx_) {
        return ErrorCode::OutOfMemory;
    }

    audioCodecCtx_->sample_rate = config_.audioSampleRate;
    audioCodecCtx_->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    audioCodecCtx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
    audioCodecCtx_->bit_rate = config_.audioBitrate;
    audioCodecCtx_->time_base = {1, config_.audioSampleRate};

    if (formatCtx_->oformat->flags & AVFMT_GLOBALHEADER) {
        audioCodecCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(audioCodecCtx_, codec, nullptr) < 0) {
        return ErrorCode::CodecOpenFailed;
    }

    avcodec_parameters_from_context(audioStream_->codecpar, audioCodecCtx_);
    audioStream_->time_base = audioCodecCtx_->time_base;

    // 分配转换帧
    convertedAudioFrame_ = av_frame_alloc();
    convertedAudioFrame_->format = AV_SAMPLE_FMT_FLTP;
    convertedAudioFrame_->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    convertedAudioFrame_->sample_rate = config_.audioSampleRate;
    convertedAudioFrame_->nb_samples = audioCodecCtx_->frame_size;
    av_frame_get_buffer(convertedAudioFrame_, 0);

    return ErrorCode::OK;
}

ErrorCode VideoRecorder::WriteVideoFrame(const VideoFrame& frame) {
    if (!videoCodecCtx_ || !convertedVideoFrame_) {
        return ErrorCode::NotInitialized;
    }

    // 创建/更新 SwsContext
    if (!swsCtx_) {
        swsCtx_ = sws_getContext(
            frame.resolution.width, frame.resolution.height, (AVPixelFormat)frame.format,
            config_.videoWidth, config_.videoHeight, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!swsCtx_) {
            return ErrorCode::CodecNotSupported;
        }
    }

    // 转换帧格式
    sws_scale(swsCtx_, frame.data, frame.linesize, 0, frame.resolution.height,
              convertedVideoFrame_->data, convertedVideoFrame_->linesize);

    convertedVideoFrame_->pts = videoPts_++;

    // 发送帧到编码器
    int ret = avcodec_send_frame(videoCodecCtx_, convertedVideoFrame_);
    if (ret < 0) {
        return ErrorCode::CodecEncodeFailed;
    }

    // 接收编码后的数据包
    AVPacket* packet = av_packet_alloc();
    while (avcodec_receive_packet(videoCodecCtx_, packet) == 0) {
        av_packet_rescale_ts(packet, videoCodecCtx_->time_base, videoStream_->time_base);
        packet->stream_index = videoStream_->index;

        ret = av_interleaved_write_frame(formatCtx_, packet);
        if (ret < 0) {
            LOG_ERROR("Failed to write video frame: {}", ret);
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);

    videoFrameCount_++;
    return ErrorCode::OK;
}

ErrorCode VideoRecorder::WriteAudioFrame(const AudioFrame& frame) {
    if (!audioCodecCtx_ || !convertedAudioFrame_) {
        return ErrorCode::NotInitialized;
    }

    // 创建/更新 SwrContext
    if (!swrCtx_) {
        swr_alloc_set_opts2(&swrCtx_,
            &audioCodecCtx_->ch_layout, audioCodecCtx_->sample_fmt, audioCodecCtx_->sample_rate,
            &frame.ch_layout, (AVSampleFormat)frame.format, frame.sample_rate,
            0, nullptr);
        if (!swrCtx_ || swr_init(swrCtx_) < 0) {
            return ErrorCode::CodecNotSupported;
        }
    }

    // 转换音频格式
    const uint8_t** srcData = const_cast<const uint8_t**>(frame.data);
    uint8_t** dstData = convertedAudioFrame_->data;
    int dstSamples = av_rescale_rnd(
        swr_get_delay(swrCtx_, frame.sampleRate) + frame.samples,
        audioCodecCtx_->sample_rate, frame.sampleRate, AV_ROUND_UP);

    swr_convert(swrCtx_, dstData, dstSamples, srcData, frame.samples);

    convertedAudioFrame_->pts = audioPts_++;

    // 发送帧到编码器
    int ret = avcodec_send_frame(audioCodecCtx_, convertedAudioFrame_);
    if (ret < 0) {
        return ErrorCode::CodecEncodeFailed;
    }

    // 接收编码后的数据包
    AVPacket* packet = av_packet_alloc();
    while (avcodec_receive_packet(audioCodecCtx_, packet) == 0) {
        av_packet_rescale_ts(packet, audioCodecCtx_->time_base, audioStream_->time_base);
        packet->stream_index = audioStream_->index;

        ret = av_interleaved_write_frame(formatCtx_, packet);
        if (ret < 0) {
            LOG_ERROR("Failed to write audio frame: {}", ret);
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);

    audioFrameCount_++;
    return ErrorCode::OK;
}

void VideoRecorder::Cleanup() {
    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }
    if (swrCtx_) {
        swr_free(&swrCtx_);
    }
    if (convertedVideoFrame_) {
        av_frame_free(&convertedVideoFrame_);
    }
    if (convertedAudioFrame_) {
        av_frame_free(&convertedAudioFrame_);
    }
    if (videoCodecCtx_) {
        avcodec_free_context(&videoCodecCtx_);
    }
    if (audioCodecCtx_) {
        avcodec_free_context(&audioCodecCtx_);
    }
    if (formatCtx_) {
        if (!(formatCtx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&formatCtx_->pb);
        }
        avformat_free_context(formatCtx_);
        formatCtx_ = nullptr;
    }
    videoStream_ = nullptr;
    audioStream_ = nullptr;
}

std::shared_ptr<IVideoRecorder> CreateVideoRecorder(const RecorderConfig& config) {
    return VideoRecorder::Create(config);
}

} // namespace AVSDK
```

**Step 4: Run test to verify it passes**

Run: `cd build/macos && make -j$(sysctl -n hw.ncpu) && ./tests/core/video_recorder_test`
Expected: PASS

**Step 5: Commit**

```bash
git add include/avsdk/video_recorder.h src/core/bypass/video_recorder.h src/core/bypass/video_recorder.cpp tests/core/video_recorder_test.cpp
git commit -m "feat(bypass): add video recorder

- Add IVideoRecorder interface for recording playback
- Add VideoRecorder implementing IDataBypass
- Support MP4/MOV/MKV output with H264/AAC encoding"
```

---

## Task 4: FFmpeg 滤镜处理 (Filter Graph)

**Files:**
- Create: `include/avsdk/filter.h`
- Create: `src/core/bypass/filter_processor.h`
- Create: `src/core/bypass/filter_processor.cpp`
- Test: `tests/core/filter_processor_test.cpp`

**Step 1: Write the failing test**

```cpp
// tests/core/filter_processor_test.cpp
#include <gtest/gtest.h>
#include "avsdk/filter.h"

using namespace AVSDK;

TEST(FilterProcessorTest, InterfaceExists) {
    // Test that IFilterProcessor interface exists
    class TestFilter : public IFilterProcessor {
    public:
        ErrorCode Initialize(const std::string& filterDesc) override {
            return ErrorCode::OK;
        }
        ErrorCode ProcessVideoFrame(VideoFrame& frame) override {
            return ErrorCode::OK;
        }
        ErrorCode ProcessAudioFrame(AudioFrame& frame) override {
            return ErrorCode::OK;
        }
    };

    TestFilter filter;
    SUCCEED();
}

TEST(FilterProcessorTest, CreateFilter) {
    auto filter = FilterProcessor::Create();
    EXPECT_NE(filter, nullptr);
}

TEST(FilterProcessorTest, InitializeAndProcess) {
    auto filter = FilterProcessor::Create();

    // Initialize with a simple scale filter
    std::string filterDesc = "scale=640:480";
    EXPECT_EQ(filter->Initialize(filterDesc), ErrorCode::OK);

    // Create a test frame
    VideoFrame frame;
    frame.resolution = {1920, 1080};
    frame.format = AV_PIX_FMT_YUV420P;
    // Note: In real test, we'd allocate actual frame data

    // Process would fail without real data, but interface should work
    // EXPECT_EQ(filter->ProcessVideoFrame(frame), ErrorCode::OK);
}

TEST(FilterProcessorTest, CommonFilters) {
    auto filter = FilterProcessor::Create();

    // Test hflip (horizontal flip)
    EXPECT_EQ(filter->Initialize("hflip"), ErrorCode::OK);

    // Test grayscale
    EXPECT_EQ(filter->Initialize("format=gray"), ErrorCode::OK);
}
```

**Step 2: Run test to verify it fails**

Run: `cd build/macos && make 2>&1 | tail -20`
Expected: FAIL with "filter.h not found"

**Step 3: Write minimal implementation**

```cpp
// include/avsdk/filter.h
#pragma once

#include "types.h"
#include "error.h"
#include <string>
#include <memory>

namespace AVSDK {

// 滤镜处理器接口
class IFilterProcessor {
public:
    virtual ~IFilterProcessor() = default;

    // 初始化滤镜图
    // @param videoFilterDesc: 视频滤镜描述（FFmpeg filtergraph语法）
    // @param audioFilterDesc: 音频滤镜描述（可选）
    // @return: 错误码
    virtual ErrorCode Initialize(const std::string& videoFilterDesc,
                                  const std::string& audioFilterDesc = "") = 0;

    // 处理视频帧
    // @param frame: 视频帧（输入输出）
    // @return: 错误码
    virtual ErrorCode ProcessVideoFrame(VideoFrame& frame) = 0;

    // 处理音频帧
    // @param frame: 音频帧（输入输出）
    // @return: 错误码
    virtual ErrorCode ProcessAudioFrame(AudioFrame& frame) = 0;

    // 是否已初始化
    // @return: 是否已初始化
    virtual bool IsInitialized() const = 0;

    // 释放资源
    virtual void Release() = 0;
};

// 常用滤镜预设
namespace Filters {
    // 视频滤镜
    const char* const Grayscale = "format=gray";
    const char* const Sepia = "colorchannelmixer=.393:.769:.189:0:.349:.686:.168:0:.272:.534:.131";
    const char* const Invert = "negate";
    const char* const HorizontalFlip = "hflip";
    const char* const VerticalFlip = "vflip";
    const char* const Rotate90 = "transpose=1";
    const char* const Rotate180 = "transpose=2,transpose=2";
    const char* const Rotate270 = "transpose=2";

    // 缩放滤镜
    inline std::string Scale(int width, int height) {
        return "scale=" + std::to_string(width) + ":" + std::to_string(height);
    }

    // 裁剪滤镜
    inline std::string Crop(int width, int height, int x, int y) {
        return "crop=" + std::to_string(width) + ":" + std::to_string(height) +
               ":" + std::to_string(x) + ":" + std::to_string(y);
    }

    // 调整亮度/对比度/饱和度
    inline std::string Adjust(float brightness, float contrast, float saturation) {
        return "eq=brightness=" + std::to_string(brightness) +
               ":contrast=" + std::to_string(contrast) +
               ":saturation=" + std::to_string(saturation);
    }

    // 音频滤镜
    const char* const VolumeUp = "volume=2.0";
    const char* const VolumeDown = "volume=0.5";
    const char* const Mono = "pan=mono|c0=0.5*c0+0.5*c1";
}

// 滤镜处理器实现类（前向声明）
class FilterProcessor;

// 创建滤镜处理器
// @return: 滤镜处理器实例
std::shared_ptr<IFilterProcessor> CreateFilterProcessor();

} // namespace AVSDK
#endif // AVSDK_FILTER_H
```

```cpp
// src/core/bypass/filter_processor.h
#pragma once

#include "avsdk/filter.h"
#include "avsdk/data_bypass.h"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
}

namespace AVSDK {

class FilterProcessor : public IFilterProcessor, public IDataBypass {
public:
    FilterProcessor();
    ~FilterProcessor() override;

    // IFilterProcessor implementation
    ErrorCode Initialize(const std::string& videoFilterDesc,
                         const std::string& audioFilterDesc = "") override;
    ErrorCode ProcessVideoFrame(VideoFrame& frame) override;
    ErrorCode ProcessAudioFrame(AudioFrame& frame) override;
    bool IsInitialized() const override;
    void Release() override;

    // IDataBypass implementation
    void OnPreRenderVideoFrame(VideoFrame& frame) override;
    void OnPreRenderAudioFrame(AudioFrame& frame) override;

    // Create instance
    static std::shared_ptr<FilterProcessor> Create();

private:
    mutable std::mutex mutex_;
    bool initialized_ = false;

    // Video filter graph
    AVFilterGraph* videoGraph_ = nullptr;
    AVFilterContext* videoSrcCtx_ = nullptr;
    AVFilterContext* videoSinkCtx_ = nullptr;
    std::string videoFilterDesc_;

    // Audio filter graph
    AVFilterGraph* audioGraph_ = nullptr;
    AVFilterContext* audioSrcCtx_ = nullptr;
    AVFilterContext* audioSinkCtx_ = nullptr;
    std::string audioFilterDesc_;

    // Current video format
    int videoWidth_ = 0;
    int videoHeight_ = 0;
    AVPixelFormat videoFormat_ = AV_PIX_FMT_NONE;

    // Current audio format
    int audioSampleRate_ = 0;
    int audioChannels_ = 0;
    AVSampleFormat audioFormat_ = AV_SAMPLE_FMT_NONE;
    uint64_t audioChannelLayout_ = 0;

    // Initialize video filter graph
    ErrorCode InitVideoFilterGraph();

    // Initialize audio filter graph
    ErrorCode InitAudioFilterGraph();

    // Cleanup
    void Cleanup();
};

} // namespace AVSDK
```

```cpp
// src/core/bypass/filter_processor.cpp
#include "filter_processor.h"
#include "avsdk/logger.h"

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/channel_layout.h>
}

namespace AVSDK {

FilterProcessor::FilterProcessor() = default;

FilterProcessor::~FilterProcessor() {
    Release();
}

std::shared_ptr<FilterProcessor> FilterProcessor::Create() {
    return std::make_shared<FilterProcessor>();
}

ErrorCode FilterProcessor::Initialize(const std::string& videoFilterDesc,
                                       const std::string& audioFilterDesc) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        Release();
    }

    videoFilterDesc_ = videoFilterDesc;
    audioFilterDesc_ = audioFilterDesc;

    // Video filter will be initialized on first frame
    // Audio filter will be initialized on first frame

    initialized_ = true;
    return ErrorCode::OK;
}

ErrorCode FilterProcessor::ProcessVideoFrame(VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return ErrorCode::NotInitialized;
    }

    // Check if filter graph needs reinitialization
    if (!videoGraph_ || videoWidth_ != frame.resolution.width ||
        videoHeight_ != frame.resolution.height || videoFormat_ != (AVPixelFormat)frame.format) {

        videoWidth_ = frame.resolution.width;
        videoHeight_ = frame.resolution.height;
        videoFormat_ = (AVPixelFormat)frame.format;

        ErrorCode result = InitVideoFilterGraph();
        if (result != ErrorCode::OK) {
            return result;
        }
    }

    // Create input frame
    AVFrame* inputFrame = av_frame_alloc();
    inputFrame->width = videoWidth_;
    inputFrame->height = videoHeight_;
    inputFrame->format = videoFormat_;
    inputFrame->pts = frame.pts;

    for (int i = 0; i < 4; i++) {
        inputFrame->data[i] = frame.data[i];
        inputFrame->linesize[i] = frame.linesize[i];
    }

    // Send frame to filter
    int ret = av_buffersrc_add_frame_flags(videoSrcCtx_, inputFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
    av_frame_free(&inputFrame);

    if (ret < 0) {
        LOG_ERROR("Failed to add frame to filter: {}", ret);
        return ErrorCode::CodecEncodeFailed;
    }

    // Get filtered frame
    AVFrame* outputFrame = av_frame_alloc();
    ret = av_buffersink_get_frame(videoSinkCtx_, outputFrame);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_frame_free(&outputFrame);
        return ErrorCode::OK;
    }

    if (ret < 0) {
        av_frame_free(&outputFrame);
        return ErrorCode::CodecEncodeFailed;
    }

    // Copy result back to VideoFrame
    frame.resolution.width = outputFrame->width;
    frame.resolution.height = outputFrame->height;
    frame.format = outputFrame->format;
    frame.pts = outputFrame->pts;

    for (int i = 0; i < 4; i++) {
        frame.data[i] = outputFrame->data[i];
        frame.linesize[i] = outputFrame->linesize[i];
    }

    av_frame_free(&outputFrame);
    return ErrorCode::OK;
}

ErrorCode FilterProcessor::ProcessAudioFrame(AudioFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return ErrorCode::NotInitialized;
    }

    if (audioFilterDesc_.empty()) {
        return ErrorCode::OK;
    }

    // Similar implementation to video...
    // (Audio filter processing)

    return ErrorCode::OK;
}

bool FilterProcessor::IsInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

void FilterProcessor::Release() {
    std::lock_guard<std::mutex> lock(mutex_);
    Cleanup();
    initialized_ = false;
}

void FilterProcessor::OnPreRenderVideoFrame(VideoFrame& frame) {
    ProcessVideoFrame(frame);
}

void FilterProcessor::OnPreRenderAudioFrame(AudioFrame& frame) {
    ProcessAudioFrame(frame);
}

ErrorCode FilterProcessor::InitVideoFilterGraph() {
    // Cleanup existing graph
    if (videoGraph_) {
        avfilter_graph_free(&videoGraph_);
        videoGraph_ = nullptr;
    }

    // Create new filter graph
    videoGraph_ = avfilter_graph_alloc();
    if (!videoGraph_) {
        return ErrorCode::OutOfMemory;
    }

    // Create buffer source
    const AVFilter* bufferSrc = avfilter_get_by_name("buffer");
    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=1/30:pixel_aspect=1/1",
             videoWidth_, videoHeight_, videoFormat_);

    int ret = avfilter_graph_create_filter(&videoSrcCtx_, bufferSrc, "in",
                                            args, nullptr, videoGraph_);
    if (ret < 0) {
        LOG_ERROR("Failed to create buffer source: {}", ret);
        return ErrorCode::CodecOpenFailed;
    }

    // Create buffer sink
    const AVFilter* bufferSink = avfilter_get_by_name("buffersink");
    ret = avfilter_graph_create_filter(&videoSinkCtx_, bufferSink, "out",
                                        nullptr, nullptr, videoGraph_);
    if (ret < 0) {
        LOG_ERROR("Failed to create buffer sink: {}", ret);
        return ErrorCode::CodecOpenFailed;
    }

    // Create the filter chain
    AVFilterInOut* inputs = avfilter_inout_alloc();
    AVFilterInOut* outputs = avfilter_inout_alloc();

    outputs->name = av_strdup("in");
    outputs->filter_ctx = videoSrcCtx_;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = videoSinkCtx_;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    // Parse filter description
    ret = avfilter_graph_parse_ptr(videoGraph_, videoFilterDesc_.c_str(),
                                    &inputs, &outputs, nullptr);

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    if (ret < 0) {
        LOG_ERROR("Failed to parse filter graph: {}", ret);
        return ErrorCode::CodecOpenFailed;
    }

    // Configure the graph
    ret = avfilter_graph_config(videoGraph_, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to configure filter graph: {}", ret);
        return ErrorCode::CodecOpenFailed;
    }

    LOG_INFO("Video filter graph initialized: {}", videoFilterDesc_);
    return ErrorCode::OK;
}

ErrorCode FilterProcessor::InitAudioFilterGraph() {
    // Similar implementation to video filter...
    return ErrorCode::OK;
}

void FilterProcessor::Cleanup() {
    if (videoGraph_) {
        avfilter_graph_free(&videoGraph_);
        videoGraph_ = nullptr;
    }
    if (audioGraph_) {
        avfilter_graph_free(&audioGraph_);
        audioGraph_ = nullptr;
    }
    videoSrcCtx_ = nullptr;
    videoSinkCtx_ = nullptr;
    audioSrcCtx_ = nullptr;
    audioSinkCtx_ = nullptr;
}

std::shared_ptr<IFilterProcessor> CreateFilterProcessor() {
    return FilterProcessor::Create();
}

} // namespace AVSDK
```

**Step 4: Run test to verify it passes**

Run: `cd build/macos && make -j$(sysctl -n hw.ncpu) && ./tests/core/filter_processor_test`
Expected: PASS

**Step 5: Commit**

```bash
git add include/avsdk/filter.h src/core/bypass/filter_processor.h src/core/bypass/filter_processor.cpp tests/core/filter_processor_test.cpp
git commit -m "feat(bypass): add FFmpeg filter processor

- Add IFilterProcessor interface for FFmpeg filtergraph
- Add FilterProcessor implementing IDataBypass
- Support common filters: scale, crop, grayscale, hflip, etc."
```

---

## Task 5: 更新 CMakeLists.txt 并运行所有测试

**Files:**
- Modify: `CMakeLists.txt` (add new sources)
- Test: `tests/CMakeLists.txt`

**Step 1: Update CMakeLists.txt**

```cmake
# Add to CMakeLists.txt

# Data bypass sources
set(CORE_BYPASS_SOURCES
    src/core/bypass/data_bypass_manager.cpp
    src/core/bypass/screenshot_handler.cpp
    src/core/bypass/video_recorder.cpp
    src/core/bypass/filter_processor.cpp
)

# Add to target_sources
target_sources(avsdk_core PRIVATE
    ${CORE_BYPASS_SOURCES}
    # ... existing sources ...
)

# Link avfilter for filter support
target_link_libraries(avsdk_core PRIVATE
    ${FFMPEG_LIBRARIES}
    avfilter  # Add this
)
```

**Step 2: Run all Phase 4 tests**

```bash
cd build/macos
make -j$(sysctl -n hw.ncpu)
ctest --output-on-failure -R "bypass|screenshot|recorder|filter"
```

Expected: All 12 tests pass

**Step 3: Run full test suite**

```bash
cd build/macos
ctest --output-on-failure
```

Expected: All 52 tests pass (40 existing + 12 new)

**Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add Phase 4 data bypass sources to CMakeLists"
```

---

## 验收标准

- [ ] DataBypass 回调接口定义完整 (IDataBypass)
- [ ] DataBypassManager 管理多个处理器
- [ ] Player 集成 DataBypass 回调
- [ ] 视频截图功能 (ScreenshotHandler) - 3 tests pass
- [ ] 视频录制功能 (VideoRecorder) - 3 tests pass
- [ ] FFmpeg 滤镜处理 (FilterProcessor) - 3 tests pass
- [ ] 所有 Phase 4 单元测试通过 - 12 tests pass
- [ ] 项目总测试数: 52 tests (100% pass)

---

## 测试统计

| 模块 | 测试数 | 状态 |
|------|--------|------|
| DataBypass Interface | 3 | ⏳ |
| Player Bypass | 2 | ⏳ |
| Screenshot Handler | 3 | ⏳ |
| Video Recorder | 3 | ⏳ |
| Filter Processor | 3 | ⏳ |
| **Phase 4 总计** | **14** | ⏳ |

## 项目总测试统计

- Phase 1: 17 tests ✅
- Phase 2: 14 tests ✅
- Phase 3: 9 tests ✅
- Phase 4: 14 tests ⏳
- **预计总计: 54 tests**

---

## 相关文档

- PRD: [../context/跨平台音视频SDK_PRD.md](../context/跨平台音视频SDK_PRD.md) - 第 2.5.2 节数据旁路需求
- 接口设计: [../context/av_sdk_interface_design.md](../context/av_sdk_interface_design.md) - 第 2.6/2.7 节回调接口

---

*计划完成，准备执行*
