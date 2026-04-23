# Performance Targets

## Playback Performance

| Metric | Target | Test Condition |
|--------|--------|----------------|
| First frame (local) | ≤ 500ms | Cold start, 1080p file |
| First frame (network) | ≤ 2s | Including buffering, HLS |
| Seek response | ≤ 200ms | Local file seek |
| Live latency | ≤ 3s | RTMP/HLS end-to-end |

## Resource Usage

| Metric | Target | Test Condition |
|--------|--------|----------------|
| 1080p@30fps CPU | < 30% | iPhone 12 equivalent |
| 1080p@60fps CPU | < 50% | iPhone 12 equivalent |
| 4K@30fps CPU | < 60% | iPhone 12 Pro equivalent |
| Memory growth | ≤ 100MB | During 1 hour playback |
| Memory at startup | ≤ 50MB | After initialization |

## Quality Metrics

| Metric | Target | Notes |
|--------|--------|-------|
| AV sync deviation | ≤ 40ms | Human perception threshold |
| Frame drop rate | < 1% | Under normal conditions |
| Audio glitch rate | < 0.1% | No audible artifacts |

## Stress Tests

| Test | Target | Duration |
|------|--------|----------|
| Continuous playback | > 24h | No memory leaks |
| Rapid seek | 1000 seeks | No crash, stable memory |
| Format switching | 100 switches | No resource leaks |
| Network fluctuation | Simulate 3G/4G/WiFi | Auto recovery |

## Platform-Specific Targets

### iOS/macOS

| Metric | iPhone 12 | iPad Pro | MacBook Pro |
|--------|-----------|----------|-------------|
| 1080p30 CPU | < 25% | < 20% | < 15% |
| 4K30 CPU | < 55% | < 45% | < 30% |
| Battery (1h) | < 10% | < 8% | N/A |

### Android

| Metric | Mid-range | High-end |
|--------|-----------|----------|
| 1080p30 CPU | < 40% | < 25% |
| 4K30 CPU | < 70% | < 50% |
| Thermal throttling | No | No |

### Windows

| Metric | Target |
|--------|--------|
| 1080p60 CPU | < 20% |
| 4K60 CPU | < 40% |
| GPU utilization | < 30% |

## Benchmarking

### Test Files

| Resolution | Codec | Format | Purpose |
|------------|-------|--------|---------|
| 1920x1080 | H264 | MP4 | Baseline |
| 1920x1080 | H265 | MP4 | Efficiency |
| 3840x2160 | H264 | MP4 | 4K test |
| 3840x2160 | H265 | MP4 | 4K efficiency |
| 1280x720 | H264 | FLV | Streaming |

### Measurement Tools

- **CPU**: Instruments (iOS), Android Profiler, Windows Performance Analyzer
- **Memory**: Xcode Memory Graph, Android Memory Profiler
- **AV Sync**: Custom test pattern with audio beep
- **Latency**: Frame timestamp analysis

## Optimization Guidelines

1. **Use hardware decoding** when available
2. **Implement zero-copy** rendering paths
3. **Pool memory** for video frames
4. **Batch operations** to reduce CPU wake-ups
5. **Adaptive buffer sizing** based on network conditions
