# 跨平台音视频SDK产品需求文档（PRD）

## 文档信息

| 项目 | 内容 |
|------|------|
| 文档版本 | v1.0 |
| 创建日期 | 2024年 |
| 文档状态 | 初稿 |
| 目标平台 | iOS/iPadOS、Android、macOS、Windows |
| 技术栈 | FFmpeg + C++ + 平台原生框架 |

---

## 1. 项目概述

### 1.1 项目背景
开发一个基于FFmpeg的跨平台音视频编解码渲染播放SDK，采用C++作为底层实现语言，通过平台适配层支持iOS（含iPadOS）、Android、macOS、Windows四个主流平台。

### 1.2 技术架构
```
┌─────────────────────────────────────────────────────────────┐
│                    应用层 (Application)                      │
├─────────────────────────────────────────────────────────────┤
│  iOS/iPadOS    │   Android    │    macOS     │   Windows    │
│  (Objective-C) │   (JNI/Java) │  (Objective-C)│   (C++/C#)   │
├─────────────────────────────────────────────────────────────┤
│              平台适配层 (Platform Adapter)                    │
├─────────────────────────────────────────────────────────────┤
│                    核心SDK (C++)                             │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │  解封装   │ │  解码器   │ │  渲染器   │ │  编码器   │       │
│  │ Demuxer  │ │  Decoder │ │ Renderer │ │  Encoder │       │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘       │
├─────────────────────────────────────────────────────────────┤
│                    FFmpeg 核心库                             │
│         (libavformat, libavcodec, libavutil等)              │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 开发策略
- **优先平台**: macOS（作为验证平台，C++→Objective-C转换容易）
- **实现顺序**: 本地播放 → 网络流播放 → 实时编码 → 数据旁路

---

## 2. 功能需求

### 2.1 播放功能需求

#### 2.1.1 本地文件播放

| 需求ID | 需求描述 | 优先级 | 备注 |
|--------|----------|--------|------|
| PLAY-001 | 支持常见视频格式：MP4、MOV、MKV、AVI、FLV、WMV | P0 | 基于FFmpeg解封装 |
| PLAY-002 | 支持常见音频格式：AAC、MP3、AC3、FLAC、WAV | P0 | - |
| PLAY-003 | 支持H264/H265视频解码 | P0 | 含软硬解码 |
| PLAY-004 | 支持多音轨切换 | P1 | - |
| PLAY-005 | 支持字幕显示（内嵌/外挂） | P2 | SRT、ASS格式 |
| PLAY-006 | 支持播放速度调节（0.5x-2.0x） | P1 | 保持音调不变 |
| PLAY-007 | 支持循环播放模式 | P1 | 单曲/列表循环 |
| PLAY-008 | 支持播放列表管理 | P2 | - |

#### 2.1.2 网络流播放

| 需求ID | 需求描述 | 优先级 | 备注 |
|--------|----------|--------|------|
| STREAM-001 | 支持HTTP/HTTPS协议播放 | P0 | - |
| STREAM-002 | 支持RTMP协议播放 | P0 | 推流/拉流 |
| STREAM-003 | 支持HLS协议播放（m3u8） | P0 | 自适应码率 |
| STREAM-004 | 支持FLV over HTTP播放 | P0 | - |
| STREAM-005 | 支持DASH协议播放 | P1 | - |
| STREAM-006 | 支持WebRTC播放 | P2 | 后续版本 |
| STREAM-007 | 支持网络自适应缓冲 | P0 | 动态调整缓冲大小 |
| STREAM-008 | 支持断线重连机制 | P0 | 可配置重试次数 |
| STREAM-009 | 支持直播时移功能 | P1 | HLS DVR |

### 2.2 编解码功能需求

#### 2.2.1 视频编解码

| 需求ID | 需求描述 | 优先级 | 备注 |
|--------|----------|--------|------|
| VDEC-001 | 支持H264软件解码 | P0 | FFmpeg libx264 |
| VDEC-002 | 支持H265/HEVC软件解码 | P0 | FFmpeg libx265 |
| VDEC-003 | 支持H264硬件解码（VideoToolbox/ MediaCodec/ DXVA/D3D11VA） | P0 | 平台特定 |
| VDEC-004 | 支持H265硬件解码（VideoToolbox/ MediaCodec/ DXVA/D3D11VA） | P0 | 平台特定 |
| VDEC-005 | 支持多线程解码 | P1 | 提升解码效率 |
| VDEC-006 | 支持丢帧策略（卡顿恢复） | P1 | 直播场景 |
| VENC-001 | 支持H264软件编码 | P0 | FFmpeg libx264 |
| VENC-002 | 支持H265软件编码 | P0 | FFmpeg libx265 |
| VENC-003 | 支持H264硬件编码（VideoToolbox/ MediaCodec/ MediaFoundation） | P0 | 平台特定 |
| VENC-004 | 支持H265硬件编码（VideoToolbox/ MediaCodec/ MediaFoundation） | P0 | 平台特定 |
| VENC-005 | 支持编码参数配置（码率、帧率、分辨率、Profile） | P0 | - |
| VENC-006 | 支持关键帧间隔设置 | P0 | GOP控制 |

#### 2.2.2 音频编解码

| 需求ID | 需求描述 | 优先级 | 备注 |
|--------|----------|--------|------|
| ADEC-001 | 支持AAC解码 | P0 | - |
| ADEC-002 | 支持MP3解码 | P0 | - |
| ADEC-003 | 支持AC3/E-AC3解码 | P1 | - |
| AENC-001 | 支持AAC编码 | P0 | - |
| AENC-002 | 支持音频重采样 | P0 | 统一输出格式 |
| AENC-003 | 支持音频增益控制 | P1 | 音量调节 |

### 2.3 渲染功能需求

#### 2.3.1 视频渲染

| 需求ID | 需求描述 | 优先级 | 备注 |
|--------|----------|--------|------|
| REND-001 | 支持OpenGL ES渲染（iOS/Android） | P0 | - |
| REND-002 | 支持Metal渲染（iOS/macOS） | P1 | 性能优化 |
| REND-003 | 支持Direct3D 11渲染（Windows） | P0 | - |
| REND-004 | 支持画面缩放模式（填充/适应/拉伸） | P0 | - |
| REND-005 | 支持画面旋转（0°/90°/180°/270°） | P1 | - |
| REND-006 | 支持亮度/对比度/饱和度调节 | P2 | - |
| REND-007 | 支持截图功能 | P1 | 导出当前帧 |
| REND-008 | 支持水印叠加 | P2 | 图片/文字水印 |

#### 2.3.2 音频渲染

| 需求ID | 需求描述 | 优先级 | 备注 |
|--------|----------|--------|------|
| AREND-001 | 支持AudioUnit渲染（iOS/macOS） | P0 | - |
| AREND-002 | 支持OpenSL ES渲染（Android） | P0 | - |
| AREND-003 | 支持AAudio渲染（Android 8.0+） | P1 | 低延迟 |
| AREND-004 | 支持WASAPI渲染（Windows） | P0 | - |
| AREND-005 | 支持音量控制（独立/系统） | P0 | - |
| AREND-006 | 支持音频焦点管理 | P1 | 来电暂停等 |

### 2.4 控制功能需求

| 需求ID | 需求描述 | 优先级 | 备注 |
|--------|----------|--------|------|
| CTRL-001 | 支持播放/暂停控制 | P0 | - |
| CTRL-002 | 支持停止/重置 | P0 | - |
| CTRL-003 | 支持Seek操作（精确/快速） | P0 | 支持关键帧/帧精确 |
| CTRL-004 | 支持Seek到指定时间戳 | P0 | 毫秒精度 |
| CTRL-005 | 支持获取当前播放时间 | P0 | - |
| CTRL-006 | 支持获取总时长 | P0 | - |
| CTRL-007 | 支持获取播放进度百分比 | P0 | - |
| CTRL-008 | 支持缓冲进度获取 | P0 | 网络流 |
| CTRL-009 | 支持预加载功能 | P1 | 播放下一个 |
| CTRL-010 | 支持后台播放（音频） | P1 | 需配置 |

### 2.5 回调和数据旁路需求

#### 2.5.1 状态回调

| 需求ID | 需求描述 | 优先级 | 备注 |
|--------|----------|--------|------|
| CB-001 | 播放状态回调（准备/播放/暂停/停止/完成/错误） | P0 | - |
| CB-002 | 缓冲状态回调（开始/进度/结束） | P0 | - |
| CB-003 | 播放进度回调（时间/百分比） | P0 | 可配置频率 |
| CB-004 | 错误回调（错误码+描述） | P0 | - |
| CB-005 | 首帧渲染回调 | P1 | - |

#### 2.5.2 数据旁路回调（第四阶段）

| 需求ID | 需求描述 | 优先级 | 备注 |
|--------|----------|--------|------|
| BYPASS-001 | 原始音视频数据回调（解封装后） | P0 | AVPacket |
| BYPASS-002 | 解码后视频帧回调（YUV/RGB） | P0 | AVFrame |
| BYPASS-003 | 解码后音频帧回调（PCM） | P0 | AVFrame |
| BYPASS-004 | 渲染前视频帧回调 | P1 | 允许修改 |
| BYPASS-005 | 渲染前音频帧回调 | P1 | 允许修改 |
| BYPASS-006 | 编码后数据回调 | P1 | AVPacket |
| BYPASS-007 | 支持自定义滤镜处理 | P2 | FFmpeg滤镜图 |

---

## 3. 非功能需求

### 3.1 性能指标

#### 3.1.1 延迟指标

| 指标项 | 目标值 | 说明 |
|--------|--------|------|
| 本地文件首帧时间 | ≤ 500ms | 从调用play到首帧渲染 |
| 网络流首帧时间 | ≤ 2s | 含缓冲时间 |
| 直播延迟 | ≤ 3s | RTMP/HLS端到端延迟 |
| Seek响应时间 | ≤ 300ms | 本地文件 |
| 音视频同步偏差 | ≤ 40ms | 符合人耳感知阈值 |

#### 3.1.2 帧率指标

| 指标项 | 目标值 | 说明 |
|--------|--------|------|
| 1080p@30fps播放 | 稳定30fps | CPU占用<30% |
| 1080p@60fps播放 | 稳定60fps | 硬件解码 |
| 4K@30fps播放 | 稳定30fps | 硬件解码 |
| 编码帧率 | ≥ 25fps | 1080p硬件编码 |

#### 3.1.3 CPU/内存占用

| 指标项 | 目标值 | 说明 |
|--------|--------|------|
| 1080p播放CPU占用 | ≤ 30% | iPhone 12同级设备 |
| 内存占用增长 | ≤ 100MB | 播放过程中 |
| 内存峰值 | ≤ 200MB | 含缓冲 |
| 后台内存占用 | ≤ 50MB | 音频播放 |

### 3.2 兼容性要求

| 平台 | 最低版本 | 说明 |
|------|----------|------|
| iOS | iOS 12.0+ | 支持iPhone 6s及更新机型 |
| iPadOS | iPadOS 12.0+ | - |
| Android | API 21 (Android 5.0) | 覆盖95%+设备 |
| macOS | macOS 10.14+ | 支持Metal |
| Windows | Windows 10 1803+ | 支持D3D11 |

### 3.3 稳定性要求

| 指标项 | 目标值 | 说明 |
|--------|--------|------|
| 崩溃率 | < 0.1% | 生产环境 |
| ANR率 | < 0.05% | Android |
| 播放成功率 | > 99% | 有效URL |
| 连续播放时长 | > 24h | 无内存泄漏 |

### 3.4 资源管理要求

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| RES-001 | 支持自动资源释放 | P0 |
| RES-002 | 支持手动资源清理 | P0 |
| RES-003 | 支持缓存管理（磁盘/内存） | P1 |
| RES-004 | 支持网络超时配置 | P0 |
| RES-005 | 支持并发播放限制 | P1 |

---

## 4. 平台特定需求

### 4.1 iOS/iPadOS特定约束

#### 4.1.1 沙盒限制

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| IOS-001 | 支持应用沙盒内文件访问 | P0 |
| IOS-002 | 支持iCloud文件访问 | P1 |
| IOS-003 | 支持相册视频访问（需授权） | P1 |

#### 4.1.2 后台播放

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| IOS-004 | 支持后台音频播放（配置Audio Background Mode） | P1 |
| IOS-005 | 支持后台画中画（PiP） | P2 |
| IOS-006 | 支持Control Center集成 | P1 |
| IOS-007 | 支持锁屏界面控制 | P1 |

#### 4.1.3 权限要求

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| IOS-008 | 麦克风权限（录制场景） | P0 |
| IOS-009 | 相机权限（录制场景） | P0 |
| IOS-010 | 相册权限（保存场景） | P1 |

#### 4.1.4 其他约束

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| IOS-011 | 支持静音开关检测 | P1 |
| IOS-012 | 支持音频路由切换（耳机/扬声器） | P1 |
| IOS-013 | 支持屏幕旋转适配 | P1 |
| IOS-014 | 支持刘海屏/灵动岛适配 | P1 |

### 4.2 Android特定约束

#### 4.2.1 版本兼容性

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| AND-001 | 支持API 21-34兼容性 | P0 |
| AND-002 | 支持Android 10分区存储 | P0 |
| AND-003 | 支持64位架构（arm64-v8a, x86_64） | P0 |
| AND-004 | 支持32位架构（armeabi-v7a, x86） | P1 |

#### 4.2.2 权限要求

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| AND-005 | 网络权限（INTERNET） | P0 |
| AND-006 | 存储权限（READ/WRITE_EXTERNAL_STORAGE） | P0 |
| AND-007 | 麦克风权限（RECORD_AUDIO） | P0 |
| AND-008 | 相机权限（CAMERA） | P0 |
| AND-009 | 前台服务权限（FOREGROUND_SERVICE） | P1 |

#### 4.2.3 生命周期管理

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| AND-010 | 支持Activity生命周期绑定 | P0 |
| AND-011 | 支持后台播放服务 | P1 |
| AND-012 | 支持通知栏控制 | P1 |
| AND-013 | 支持音频焦点管理 | P1 |

#### 4.2.4 其他约束

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| AND-014 | 支持不同厂商兼容性（华为/小米/OPPO/vivo等） | P1 |
| AND-015 | 支持刘海屏适配 | P1 |
| AND-016 | 支持分屏/多窗口模式 | P2 |

### 4.3 macOS特定约束

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| MAC-001 | 支持macOS 10.14+ | P0 |
| MAC-002 | 支持Apple Silicon（M1/M2/M3） | P0 |
| MAC-003 | 支持Intel芯片 | P0 |
| MAC-004 | 支持Metal渲染 | P1 |
| MAC-005 | 支持多显示器适配 | P2 |
| MAC-006 | 支持Touch Bar控制 | P2 |
| MAC-007 | 支持沙盒模式（App Store发布） | P1 |

### 4.4 Windows特定约束

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| WIN-001 | 支持Windows 10 1803+ | P0 |
| WIN-002 | 支持Windows 11 | P0 |
| WIN-003 | 支持x64架构 | P0 |
| WIN-004 | 支持Direct3D 11渲染 | P0 |
| WIN-005 | 支持Direct3D 12渲染 | P2 |
| WIN-006 | 支持多显示器适配 | P2 |
| WIN-007 | 支持高DPI适配 | P1 |

---

## 5. 各阶段详细任务分解

### 5.1 第一阶段：本地音视频文件播放

#### 5.1.1 功能列表

| 模块 | 功能项 | 说明 |
|------|--------|------|
| 解封装 | 支持MP4/MOV/MKV/AVI格式 | FFmpeg demuxer |
| 解码 | H264/H265软解 | FFmpeg decoder |
| 解码 | AAC/MP3音频解码 | FFmpeg decoder |
| 渲染 | OpenGL ES视频渲染 | 平台适配层 |
| 渲染 | 音频渲染 | AudioUnit/OpenSL/WASAPI |
| 控制 | 播放/暂停/停止 | 基础控制 |
| 控制 | Seek操作 | 关键帧Seek |
| 回调 | 播放状态回调 | 状态机管理 |

#### 5.1.2 交付物

| 交付物 | 说明 |
|--------|------|
| SDK核心库 | libffplayer_core.a/.so/.lib |
| macOS Demo | 完整的macOS播放器Demo |
| API文档 | C++接口文档和平台适配文档 |
| 单元测试 | 核心模块单元测试用例 |

#### 5.1.3 验收标准

| 验收项 | 标准 |
|--------|------|
| 功能验收 | 支持MP4/MOV本地文件正常播放 |
| 功能验收 | 支持H264/H265视频正常解码显示 |
| 功能验收 | 支持AAC/MP3音频正常播放 |
| 功能验收 | 播放/暂停/停止/Seek功能正常 |
| 性能验收 | 1080p视频播放CPU占用<50% |
| 性能验收 | 首帧时间<1s |
| 稳定性验收 | 连续播放1小时无崩溃/内存泄漏 |

### 5.2 第二阶段：在线音视频及串流播放

#### 5.2.1 功能列表

| 模块 | 功能项 | 说明 |
|------|--------|------|
| 网络 | HTTP/HTTPS协议支持 | FFmpeg http/https |
| 网络 | RTMP协议支持 | FFmpeg librtmp |
| 网络 | HLS协议支持 | FFmpeg hls |
| 网络 | FLV over HTTP支持 | FFmpeg httpflv |
| 缓冲 | 自适应缓冲策略 | 动态调整 |
| 缓冲 | 断线重连机制 | 可配置重试 |
| 控制 | 缓冲进度获取 | 回调通知 |
| 解码 | 硬件解码支持 | VideoToolbox/MediaCodec/DXVA |

#### 5.2.2 交付物

| 交付物 | 说明 |
|--------|------|
| 更新SDK | 含网络播放功能的新版本 |
| 网络Demo | 支持RTMP/HLS播放的Demo |
| 测试报告 | 网络播放性能测试报告 |
| 集成文档 | 网络播放集成指南 |

#### 5.2.3 验收标准

| 验收项 | 标准 |
|--------|------|
| 功能验收 | HTTP视频URL正常播放 |
| 功能验收 | RTMP直播流正常播放 |
| 功能验收 | HLS(m3u8)正常播放 |
| 功能验收 | 断线后自动重连成功 |
| 性能验收 | 网络流首帧时间<3s |
| 性能验收 | 直播延迟<5s |
| 稳定性验收 | 连续播放直播4小时稳定 |

### 5.3 第三阶段：实时编码音视频

#### 5.3.1 功能列表

| 模块 | 功能项 | 说明 |
|------|--------|------|
| 采集 | 摄像头采集 | 平台原生API |
| 采集 | 麦克风采集 | 平台原生API |
| 编码 | H264/H265软编码 | FFmpeg encoder |
| 编码 | H264/H265硬编码 | VideoToolbox/MediaCodec/MF |
| 编码 | AAC音频编码 | FFmpeg encoder |
| 封装 | MP4封装 | FFmpeg muxer |
| 封装 | FLV封装 | FFmpeg muxer |
| 推流 | RTMP推流 | FFmpeg rtmp |

#### 5.3.2 交付物

| 交付物 | 说明 |
|--------|------|
| 完整SDK | 含编码推流功能 |
| 推流Demo | 摄像头推流Demo |
| API文档 | 编码推流接口文档 |
| 性能报告 | 编码性能测试报告 |

#### 5.3.3 验收标准

| 验收项 | 标准 |
|--------|------|
| 功能验收 | 摄像头预览正常 |
| 功能验收 | H264硬编码1080p@30fps成功 |
| 功能验收 | RTMP推流成功 |
| 性能验收 | 编码帧率稳定25fps+ |
| 性能验收 | 编码CPU占用<40% |
| 稳定性验收 | 连续推流1小时稳定 |

### 5.4 第四阶段：音视频数据旁路回调

#### 5.4.1 功能列表

| 模块 | 功能项 | 说明 |
|------|--------|------|
| 旁路 | 原始数据回调 | AVPacket级别 |
| 旁路 | 解码后视频帧回调 | YUV/RGB格式 |
| 旁路 | 解码后音频帧回调 | PCM格式 |
| 旁路 | 渲染前数据回调 | 允许修改 |
| 旁路 | 编码后数据回调 | AVPacket级别 |
| 滤镜 | FFmpeg滤镜支持 | 滤镜图处理 |
| 处理 | 自定义处理接口 | 用户可注入处理 |

#### 5.4.2 交付物

| 交付物 | 说明 |
|--------|------|
| 完整SDK | 含数据旁路功能 |
| 旁路Demo | 数据旁路使用Demo |
| 滤镜Demo | 自定义滤镜处理Demo |
| 完整文档 | 所有功能完整文档 |

#### 5.4.3 验收标准

| 验收项 | 标准 |
|--------|------|
| 功能验收 | 原始数据回调正常 |
| 功能验收 | 解码后视频帧回调正常 |
| 功能验收 | 解码后音频帧回调正常 |
| 功能验收 | 自定义滤镜处理正常 |
| 性能验收 | 旁路回调不影响播放性能 |
| 稳定性验收 | 旁路功能长时间运行稳定 |

---

## 6. 边界情况和异常处理需求

### 6.1 网络异常处理

| 异常场景 | 处理策略 | 回调通知 |
|----------|----------|----------|
| 网络断开 | 自动重连（可配置次数） | 错误码：NETWORK_DISCONNECTED |
| 连接超时 | 重试或报错 | 错误码：CONNECT_TIMEOUT |
| 读取超时 | 继续缓冲或报错 | 错误码：READ_TIMEOUT |
| 404/403错误 | 停止播放并报错 | 错误码：HTTP_ERROR |
| 网络切换 | 自动恢复播放 | 状态：RECONNECTING |
| 弱网环境 | 降低码率或暂停缓冲 | 状态：BUFFERING |

### 6.2 解码失败处理

| 异常场景 | 处理策略 | 回调通知 |
|----------|----------|----------|
| 不支持的格式 | 报错并停止 | 错误码：UNSUPPORTED_FORMAT |
| 解码器初始化失败 | 尝试软解回退 | 警告：DECODER_FALLBACK |
| 解码帧错误 | 跳过错误帧 | 警告：FRAME_SKIP |
| 硬件解码失败 | 自动切换到软解 | 警告：HW_DECODER_FAILED |
| 音画不同步 | 自动校正同步 | 内部处理 |

### 6.3 资源不足处理

| 异常场景 | 处理策略 | 回调通知 |
|----------|----------|----------|
| 内存不足 | 降低缓冲/释放缓存 | 警告：LOW_MEMORY |
| 磁盘空间不足 | 停止录制/缓存 | 错误码：DISK_FULL |
| CPU过载 | 降帧/降分辨率 | 警告：HIGH_CPU_USAGE |
| 句柄耗尽 | 清理资源 | 错误码：RESOURCE_EXHAUSTED |

### 6.4 其他异常处理

| 异常场景 | 处理策略 | 回调通知 |
|----------|----------|----------|
| 文件不存在 | 报错 | 错误码：FILE_NOT_FOUND |
| 文件损坏 | 尝试修复或报错 | 错误码：FILE_CORRUPTED |
| 权限不足 | 报错 | 错误码：PERMISSION_DENIED |
| 未知错误 | 记录日志并报错 | 错误码：UNKNOWN_ERROR |

---

## 7. 错误码定义

### 7.1 错误码分类

```
错误码格式：0x[类别][具体码]
类别：
  01 - 通用错误
  02 - 网络错误
  03 - 解码错误
  04 - 编码错误
  05 - 渲染错误
  06 - 文件错误
  07 - 权限错误
  08 - 资源错误
```

### 7.2 详细错误码

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0x010001 | SUCCESS | 成功 |
| 0x010002 | UNKNOWN_ERROR | 未知错误 |
| 0x010003 | INVALID_PARAMETER | 无效参数 |
| 0x010004 | NOT_INITIALIZED | 未初始化 |
| 0x020001 | NETWORK_DISCONNECTED | 网络断开 |
| 0x020002 | CONNECT_TIMEOUT | 连接超时 |
| 0x020003 | READ_TIMEOUT | 读取超时 |
| 0x020004 | HTTP_ERROR | HTTP错误 |
| 0x030001 | UNSUPPORTED_FORMAT | 不支持的格式 |
| 0x030002 | DECODER_INIT_FAILED | 解码器初始化失败 |
| 0x030003 | DECODE_FAILED | 解码失败 |
| 0x030004 | HW_DECODER_FAILED | 硬件解码失败 |
| 0x040001 | ENCODER_INIT_FAILED | 编码器初始化失败 |
| 0x040002 | ENCODE_FAILED | 编码失败 |
| 0x050001 | RENDER_INIT_FAILED | 渲染器初始化失败 |
| 0x060001 | FILE_NOT_FOUND | 文件不存在 |
| 0x060002 | FILE_CORRUPTED | 文件损坏 |
| 0x070001 | PERMISSION_DENIED | 权限不足 |
| 0x080001 | OUT_OF_MEMORY | 内存不足 |
| 0x080002 | DISK_FULL | 磁盘已满 |

---

## 8. 接口设计概览

### 8.1 C++核心接口

```cpp
// 播放器接口
class IFFPlayer {
public:
    // 生命周期
    virtual int Initialize(const PlayerConfig& config) = 0;
    virtual int Release() = 0;
    
    // 播放控制
    virtual int Open(const char* url) = 0;
    virtual int Play() = 0;
    virtual int Pause() = 0;
    virtual int Stop() = 0;
    virtual int Seek(int64_t position_ms) = 0;
    
    // 状态获取
    virtual PlayerState GetState() = 0;
    virtual int64_t GetCurrentPosition() = 0;
    virtual int64_t GetDuration() = 0;
    virtual int GetBufferPercentage() = 0;
    
    // 设置
    virtual int SetVolume(float volume) = 0;
    virtual int SetSpeed(float speed) = 0;
    virtual int SetRenderView(void* view) = 0;
    
    // 回调设置
    virtual int SetCallback(IFFPlayerCallback* callback) = 0;
    virtual int SetDataBypass(IDataBypass* bypass) = 0;
};

// 回调接口
class IFFPlayerCallback {
public:
    virtual void OnStateChanged(PlayerState state) = 0;
    virtual void OnProgress(int64_t position_ms, int64_t duration_ms) = 0;
    virtual void OnBuffering(int percentage) = 0;
    virtual void OnError(int errorCode, const char* message) = 0;
    virtual void OnFirstFrameRendered() = 0;
};

// 数据旁路接口
class IDataBypass {
public:
    virtual void OnRawPacket(AVPacket* packet) = 0;
    virtual void OnDecodedVideoFrame(AVFrame* frame) = 0;
    virtual void OnDecodedAudioFrame(AVFrame* frame) = 0;
    virtual void OnPreRenderVideoFrame(AVFrame* frame) = 0;
    virtual void OnPreRenderAudioFrame(AVFrame* frame) = 0;
};
```

### 8.2 平台适配接口

```objc
// iOS/macOS Objective-C接口
@interface FFPlayer : NSObject
- (instancetype)initWithConfig:(FFPlayerConfig *)config;
- (BOOL)open:(NSString *)url;
- (BOOL)play;
- (BOOL)pause;
- (BOOL)stop;
- (BOOL)seekTo:(NSTimeInterval)position;
- (void)setRenderView:(UIView/NSView *)view;
@property (nonatomic, weak) id<FFPlayerDelegate> delegate;
@end
```

```java
// Android Java接口
public class FFPlayer {
    public FFPlayer(Context context, PlayerConfig config);
    public boolean open(String url);
    public boolean play();
    public boolean pause();
    public boolean stop();
    public boolean seekTo(long positionMs);
    public void setSurface(Surface surface);
    public void setPlayerListener(PlayerListener listener);
}
```

---

## 9. 附录

### 9.1 术语表

| 术语 | 说明 |
|------|------|
| FFmpeg | 开源音视频处理框架 |
| H264/AVC | 高级视频编码标准 |
| H265/HEVC | 高效视频编码标准 |
| RTMP | 实时消息传输协议 |
| HLS | HTTP直播流协议 |
| FLV | Flash视频格式 |
| YUV | 视频颜色空间格式 |
| PCM | 脉冲编码调制（音频） |
| GOP | 图像组 |
| VideoToolbox | Apple硬件编解码框架 |
| MediaCodec | Android硬件编解码API |
| DXVA | DirectX视频加速 |

### 9.2 参考文档

- FFmpeg官方文档: https://ffmpeg.org/documentation.html
- VideoToolbox文档: Apple Developer Documentation
- MediaCodec文档: Android Developer Documentation
- DirectX文档: Microsoft Documentation

---

**文档结束**
