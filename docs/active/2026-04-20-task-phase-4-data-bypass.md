# 任务: Phase 4: 数据旁路回调

## 基本信息
- 创建时间: 2026-04-20
- 完成时间: 待完成
- 负责人: Claude
- 优先级: 高
- **状态**: 🔄 进行中

## 任务描述
实现完整的数据旁路回调系统，支持 AVPacket/AVFrame 级别数据回调，允许用户在播放流程的各个阶段注入自定义处理逻辑（如截图、录制、滤镜处理）。

## 相关文档
- 计划文档: [../plans/2026-04-20-phase4-data-bypass.md](../plans/2026-04-20-phase4-data-bypass.md)
- PRD: [../context/跨平台音视频SDK_PRD.md](../context/跨平台音视频SDK_PRD.md) - 第 2.5.2 节数据旁路需求
- 接口设计: [../context/av_sdk_interface_design.md](../context/av_sdk_interface_design.md) - 第 2.6/2.7 节回调接口

## 实施计划

### ⏳ Task 0: DataBypass 回调接口定义
- include/avsdk/data_bypass.h
- src/core/bypass/data_bypass_manager.cpp
- tests/core/data_bypass_test.cpp
- 支持 IDataBypass 接口
- 支持 DataBypassManager 管理多个处理器

### ⏳ Task 1: 集成 DataBypass 到 Player
- include/avsdk/player.h (添加 SetDataBypass 方法)
- src/core/player/player_impl.h
- src/core/player/player_impl.cpp
- tests/core/player_bypass_test.cpp

### ⏳ Task 2: 视频截图功能 (Screenshot)
- include/avsdk/screenshot.h
- src/core/bypass/screenshot_handler.h/cpp
- tests/core/screenshot_handler_test.cpp
- 支持 PNG/JPG/BMP 输出

### ⏳ Task 3: 视频录制功能 (Video Recorder)
- include/avsdk/video_recorder.h
- src/core/bypass/video_recorder.h/cpp
- tests/core/video_recorder_test.cpp
- 支持 MP4/MOV/MKV 输出

### ⏳ Task 4: FFmpeg 滤镜处理 (Filter Graph)
- include/avsdk/filter.h
- src/core/bypass/filter_processor.h/cpp
- tests/core/filter_processor_test.cpp
- 支持 scale, crop, grayscale, hflip 等滤镜

### ⏳ Task 5: 更新 CMakeLists.txt 并运行所有测试
- 添加新源文件到构建
- 运行所有 Phase 4 测试
- 验证项目总测试数

## 验收标准
- [ ] DataBypass 回调接口定义完整
- [ ] DataBypassManager 管理多个处理器
- [ ] Player 集成 DataBypass 回调
- [ ] 视频截图功能 - 3 tests pass
- [ ] 视频录制功能 - 3 tests pass
- [ ] FFmpeg 滤镜处理 - 3 tests pass
- [ ] 所有 Phase 4 单元测试通过 - 14 tests pass
- [ ] 项目总测试数: 54 tests (100% pass)

## 进展记录
- 2026-04-20: 创建 Phase 4 计划和任务文档

## 预计 Git 提交记录
```
feat(bypass): add data bypass callback interface and manager
feat(player): integrate data bypass callbacks
feat(bypass): add screenshot handler
feat(bypass): add video recorder
feat(bypass): add FFmpeg filter processor
build: add Phase 4 data bypass sources to CMakeLists
```

## 测试统计

### Phase 4 预计测试数: 14 tests

| 模块 | 测试数 | 状态 |
|------|--------|------|
| DataBypass Interface | 3 | ⏳ |
| Player Bypass | 2 | ⏳ |
| Screenshot Handler | 3 | ⏳ |
| Video Recorder | 3 | ⏳ |
| Filter Processor | 3 | ⏳ |

### 项目总测试统计
- Phase 1: 17 tests ✅
- Phase 2: 14 tests ✅
- Phase 3: 9 tests ✅
- Phase 4: 14 tests ⏳
- **预计总计: 54 tests**

## 遇到的问题
待记录...

## 总结
Phase 4 计划已创建，准备执行数据旁路回调系统实现。

**下一步**: 执行 Task 0 - DataBypass 回调接口定义

