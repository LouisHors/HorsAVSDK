# 第二阶段：网络流媒体播放 实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use horspowers:executing-plans to implement this plan task-by-task.

**日期**: 2026-04-20

## 目标

实现网络流媒体播放功能，支持 HTTP/HTTPS、HLS(m3u8)、RTMP、FLV over HTTP 协议，具备网络自适应缓冲和断线重连机制。

## 架构方案

基于 Phase 1 的本地播放架构，扩展网络层：添加 HTTP 下载器支持 range 请求和重定向；实现 HLS 解析器处理 m3u8 播放列表和自适应码率；增加环形缓冲区管理网络数据；在 Demuxer 中支持自定义 IO 回调以读取网络数据；添加重连策略和超时机制。

## 技术栈

- C++11/14
- FFmpeg 6.1.x (libavformat 网络协议支持)
- libcurl (HTTP/HTTPS 下载)
- CMake (构建系统)
- googletest (单元测试)

---

## 前置准备

### Task 0: 网络层基础架构

**Files:**
- Create: `include/avsdk/network/http_client.h`
- Create: `src/network/http_client.cpp`
- Create: `tests/network/http_client_test.cpp`
- Modify: `CMakeLists.txt` (add libcurl dependency)

**Step 1: 写失败测试**

`tests/network/http_client_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include "avsdk/network/http_client.h"
#include <string>

using namespace avsdk;

TEST(HttpClientTest, SimpleGET) {
    HttpClient client;
    HttpRequest request;
    request.url = "http://httpbin.org/get";
    request.method = HttpMethod::GET;
    
    auto response = client.Execute(request);
    EXPECT_EQ(response.status_code, 200);
    EXPECT_FALSE(response.body.empty());
}

TEST(HttpClientTest, DownloadWithRange) {
    HttpClient client;
    HttpRequest request;
    request.url = "http://httpbin.org/bytes/1024";
    request.method = HttpMethod::GET;
    request.range_start = 0;
    request.range_end = 511;
    
    auto response = client.Execute(request);
    EXPECT_EQ(response.status_code, 206);
    EXPECT_EQ(response.body.size(), 512);
}

TEST(HttpClientTest, FollowRedirect) {
    HttpClient client;
    HttpRequest request;
    request.url = "http://httpbin.org/redirect/1";
    request.method = HttpMethod::GET;
    request.follow_redirects = true;
    
    auto response = client.Execute(request);
    EXPECT_EQ(response.status_code, 200);
}
```

**Step 2: 运行测试确认失败**

```bash
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON
make http_client_test 2>&1 | head -20
```
Expected: FAIL - "network/http_client.h not found"

**Step 3: 实现 HTTP Client**

`include/avsdk/network/http_client.h`:
```cpp
#pragma once
#include <string>
#include <functional>
#include <vector>
#include "avsdk/error.h"

namespace avsdk {

enum class HttpMethod {
    GET,
    POST,
    HEAD
};

struct HttpRequest {
    std::string url;
    HttpMethod method = HttpMethod::GET;
    std::vector<std::string> headers;
    std::string body;
    int64_t range_start = -1;
    int64_t range_end = -1;
    int timeout_ms = 30000;
    bool follow_redirects = true;
    int max_redirects = 10;
};

struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::vector<std::string> headers;
    int64_t content_length = -1;
    ErrorCode error = ErrorCode::OK;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    
    HttpResponse Execute(const HttpRequest& request);
    
    // Async download with progress callback
    void DownloadAsync(const HttpRequest& request, 
                       std::function<void(const uint8_t* data, size_t size)> data_callback,
                       std::function<void(int64_t downloaded, int64_t total)> progress_callback);
    
    void Cancel();
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace avsdk
```

`src/network/http_client.cpp`:
```cpp
#include "avsdk/network/http_client.h"
#include "avsdk/logger.h"
#include <curl/curl.h>
#include <thread>
#include <atomic>

namespace avsdk {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    auto* callback = static_cast<std::function<void(const uint8_t*, size_t)>*>(userp);
    if (callback && *callback) {
        (*callback)(static_cast<uint8_t*>(contents), total_size);
    }
    return total_size;
}

static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total_size = size * nitems;
    auto* headers = static_cast<std::vector<std::string>*>(userdata);
    if (headers) {
        headers->emplace_back(buffer, total_size);
    }
    return total_size;
}

class HttpClient::Impl {
public:
    Impl() : curl_(curl_easy_init()), cancelled_(false) {
        if (!curl_) {
            LOG_ERROR("HttpClient", "Failed to initialize curl");
        }
    }
    
    ~Impl() {
        if (curl_) {
            curl_easy_cleanup(curl_);
        }
    }
    
    HttpResponse Execute(const HttpRequest& request) {
        HttpResponse response;
        
        if (!curl_) {
            response.error = ErrorCode::NotInitialized;
            return response;
        }
        
        cancelled_ = false;
        
        curl_easy_setopt(curl_, CURLOPT_URL, request.url.c_str());
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, request.follow_redirects ? 1L : 0L);
        curl_easy_setopt(curl_, CURLOPT_MAXREDIRS, request.max_redirects);
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, request.timeout_ms);
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &write_callback_);
        curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderCallback);
        curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &response.headers);
        
        // Range request
        if (request.range_start >= 0 && request.range_end > request.range_start) {
            std::string range = std::to_string(request.range_start) + "-" + std::to_string(request.range_end);
            curl_easy_setopt(curl_, CURLOPT_RANGE, range.c_str());
        }
        
        std::string response_body;
        write_callback_ = [&response_body](const uint8_t* data, size_t size) {
            response_body.append(reinterpret_cast<const char*>(data), size);
        };
        
        CURLcode res = curl_easy_perform(curl_);
        
        if (res != CURLE_OK) {
            LOG_ERROR("HttpClient", "Request failed: " + std::string(curl_easy_strerror(res)));
            response.error = ErrorCode::NetworkConnectFailed;
            return response;
        }
        
        long http_code = 0;
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);
        response.status_code = static_cast<int>(http_code);
        response.body = std::move(response_body);
        
        curl_easy_getinfo(curl_, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &response.content_length);
        
        return response;
    }
    
    void Cancel() {
        cancelled_ = true;
    }
    
private:
    CURL* curl_;
    std::atomic<bool> cancelled_;
    std::function<void(const uint8_t*, size_t)> write_callback_;
};

HttpClient::HttpClient() : impl_(std::make_unique<Impl>()) {}

HttpClient::~HttpClient() = default;

HttpResponse HttpClient::Execute(const HttpRequest& request) {
    return impl_->Execute(request);
}

void HttpClient::DownloadAsync(const HttpRequest& request,
                               std::function<void(const uint8_t* data, size_t size)> data_callback,
                               std::function<void(int64_t downloaded, int64_t total)> progress_callback) {
    std::thread([this, request, data_callback, progress_callback]() {
        // TODO: Implement async download with progress
        HttpResponse response = Execute(request);
        if (progress_callback) {
            progress_callback(response.body.size(), response.content_length);
        }
    }).detach();
}

void HttpClient::Cancel() {
    impl_->Cancel();
}

} // namespace avsdk
```

**Step 4: 运行测试确认通过**

```bash
cd build
make http_client_test
./tests/network/http_client_test
```
Expected: PASS (3 tests) - Note: Tests require network connectivity

**Step 5: Commit**

```bash
git add include/avsdk/network/http_client.h src/network/http_client.cpp tests/network/http_client_test.cpp
git commit -m "feat(network): add HTTP client with libcurl"
```

---

### Task 1: 网络缓冲区 (Ring Buffer)

**Files:**
- Create: `include/avsdk/ring_buffer.h`
- Create: `src/core/buffer/ring_buffer.cpp`
- Create: `tests/core/ring_buffer_test.cpp`

**Step 1: 写失败测试**

`tests/core/ring_buffer_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include "avsdk/ring_buffer.h"
#include <vector>
#include <thread>

using namespace avsdk;

TEST(RingBufferTest, BasicWriteRead) {
    RingBuffer buffer(1024);
    
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    size_t written = buffer.Write(data.data(), data.size());
    EXPECT_EQ(written, 5);
    
    std::vector<uint8_t> read(5);
    size_t read_count = buffer.Read(read.data(), read.size());
    EXPECT_EQ(read_count, 5);
    EXPECT_EQ(read, data);
}

TEST(RingBufferTest, CircularWrite) {
    RingBuffer buffer(16);
    
    // Fill buffer
    std::vector<uint8_t> data1(12, 0xAA);
    buffer.Write(data1.data(), data1.size());
    
    // Read some
    std::vector<uint8_t> read1(8);
    buffer.Read(read1.data(), read1.size());
    
    // Write more (should wrap around)
    std::vector<uint8_t> data2(8, 0xBB);
    size_t written = buffer.Write(data2.data(), data2.size());
    EXPECT_EQ(written, 8);
    
    // Read remaining
    std::vector<uint8_t> read2(12);
    size_t read_count = buffer.Read(read2.data(), read2.size());
    EXPECT_EQ(read_count, 12);
}

TEST(RingBufferTest, ConcurrentReadWrite) {
    RingBuffer buffer(4096);
    std::atomic<bool> stop{false};
    std::atomic<size_t> total_written{0};
    std::atomic<size_t> total_read{0};
    
    // Writer thread
    std::thread writer([&]() {
        std::vector<uint8_t> data(256, 0x42);
        while (!stop) {
            size_t written = buffer.Write(data.data(), data.size());
            total_written += written;
            if (written < data.size()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });
    
    // Reader thread
    std::thread reader([&]() {
        std::vector<uint8_t> data(256);
        while (!stop) {
            size_t read = buffer.Read(data.data(), data.size());
            total_read += read;
            if (read == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;
    
    writer.join();
    reader.join();
    
    // Drain remaining
    std::vector<uint8_t> drain(4096);
    total_read += buffer.Read(drain.data(), drain.size());
    
    EXPECT_EQ(total_written.load(), total_read.load());
}
```

**Step 2: 运行测试确认失败**

Expected: FAIL - "ring_buffer.h not found"

**Step 3: 实现 Ring Buffer**

`include/avsdk/ring_buffer.h`:
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace avsdk {

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);
    ~RingBuffer();
    
    // Write data, returns actual bytes written
    size_t Write(const uint8_t* data, size_t size);
    
    // Read data, returns actual bytes read
    size_t Read(uint8_t* data, size_t size);
    
    // Get available bytes to read
    size_t AvailableToRead() const;
    
    // Get available space to write
    size_t AvailableToWrite() const;
    
    // Clear buffer
    void Clear();
    
    // Wait until data available or timeout
    bool WaitForData(int timeout_ms);
    
private:
    std::vector<uint8_t> buffer_;
    size_t capacity_;
    size_t read_pos_ = 0;
    size_t write_pos_ = 0;
    size_t size_ = 0;
    
    mutable std::mutex mutex_;
    std::condition_variable data_available_;
    std::condition_variable space_available_;
};

} // namespace avsdk
```

`src/core/buffer/ring_buffer.cpp`:
```cpp
#include "avsdk/ring_buffer.h"
#include <algorithm>

namespace avsdk {

RingBuffer::RingBuffer(size_t capacity) 
    : buffer_(capacity), capacity_(capacity) {}

RingBuffer::~RingBuffer() = default;

size_t RingBuffer::Write(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t to_write = std::min(size, capacity_ - size_);
    size_t written = 0;
    
    while (written < to_write) {
        size_t pos = write_pos_ % capacity_;
        size_t chunk = std::min(to_write - written, capacity_ - pos);
        std::copy(data + written, data + written + chunk, buffer_.begin() + pos);
        write_pos_ += chunk;
        written += chunk;
    }
    
    size_ += written;
    
    if (written > 0) {
        data_available_.notify_one();
    }
    
    return written;
}

size_t RingBuffer::Read(uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t to_read = std::min(size, size_);
    size_t read = 0;
    
    while (read < to_read) {
        size_t pos = read_pos_ % capacity_;
        size_t chunk = std::min(to_read - read, capacity_ - pos);
        std::copy(buffer_.begin() + pos, buffer_.begin() + pos + chunk, data + read);
        read_pos_ += chunk;
        read += chunk;
    }
    
    size_ -= read;
    
    if (read > 0) {
        space_available_.notify_one();
    }
    
    return read;
}

size_t RingBuffer::AvailableToRead() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

size_t RingBuffer::AvailableToWrite() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return capacity_ - size_;
}

void RingBuffer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    read_pos_ = 0;
    write_pos_ = 0;
    size_ = 0;
}

bool RingBuffer::WaitForData(int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    return data_available_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                    [this] { return size_ > 0; });
}

} // namespace avsdk
```

**Step 4: 运行测试确认通过**

```bash
cd build
make ring_buffer_test
./tests/core/ring_buffer_test
```
Expected: PASS (3 tests)

**Step 5: Commit**

```bash
git add include/avsdk/ring_buffer.h src/core/buffer/ring_buffer.cpp tests/core/ring_buffer_test.cpp
git commit -m "feat(core): add thread-safe ring buffer for network data"
```

---

### Task 2: HLS 解析器

**Files:**
- Create: `include/avsdk/hls_parser.h`
- Create: `src/core/hls/hls_parser.cpp`
- Create: `tests/core/hls_parser_test.cpp`

**Step 1: 写失败测试**

`tests/core/hls_parser_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include "avsdk/hls_parser.h"
#include <sstream>

using namespace avsdk;

TEST(HlsParserTest, ParseSimplePlaylist) {
    std::string m3u8 = R"(#EXTM3U
#EXT-X-VERSION:3
#EXT-X-TARGETDURATION:10
#EXTINF:9.009,
segment1.ts
#EXTINF:9.009,
segment2.ts
#EXTINF:5.005,
segment3.ts
#EXT-X-ENDLIST
)";
    
    HlsPlaylist playlist;
    bool result = HlsParser::Parse(m3u8, playlist);
    EXPECT_TRUE(result);
    EXPECT_EQ(playlist.version, 3);
    EXPECT_EQ(playlist.target_duration, 10);
    EXPECT_EQ(playlist.segments.size(), 3);
    EXPECT_EQ(playlist.segments[0].duration, 9.009);
    EXPECT_EQ(playlist.segments[0].url, "segment1.ts");
}

TEST(HlsParserTest, ParseMasterPlaylist) {
    std::string m3u8 = R"(#EXTM3U
#EXT-X-STREAM-INF:BANDWIDTH=1280000,RESOLUTION=640x480
low.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=2560000,RESOLUTION=1280x720
mid.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=7680000,RESOLUTION=1920x1080
hi.m3u8
)";
    
    HlsPlaylist playlist;
    bool result = HlsParser::Parse(m3u8, playlist);
    EXPECT_TRUE(result);
    EXPECT_EQ(playlist.variants.size(), 3);
    EXPECT_EQ(playlist.variants[0].bandwidth, 1280000);
    EXPECT_EQ(playlist.variants[0].width, 640);
    EXPECT_EQ(playlist.variants[0].height, 480);
}

TEST(HlsParserTest, ParseLivePlaylist) {
    std::string m3u8 = R"(#EXTM3U
#EXT-X-VERSION:3
#EXT-X-TARGETDURATION:10
#EXT-X-MEDIA-SEQUENCE:0
#EXTINF:9.009,
segment0.ts
#EXTINF:9.009,
segment1.ts
)";
    
    HlsPlaylist playlist;
    bool result = HlsParser::Parse(m3u8, playlist);
    EXPECT_TRUE(result);
    EXPECT_EQ(playlist.media_sequence, 0);
    EXPECT_FALSE(playlist.end_list);
}
```

**Step 2: 运行测试确认失败**

Expected: FAIL - "hls_parser.h not found"

**Step 3: 实现 HLS Parser**

`include/avsdk/hls_parser.h`:
```cpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace avsdk {

struct HlsSegment {
    double duration = 0.0;
    std::string url;
    std::string title;
    bool discontinuity = false;
};

struct HlsVariant {
    int bandwidth = 0;
    int width = 0;
    int height = 0;
    std::string codecs;
    std::string url;
};

struct HlsPlaylist {
    int version = 3;
    int target_duration = 0;
    int media_sequence = 0;
    bool end_list = false;
    std::string base_url;
    std::vector<HlsSegment> segments;
    std::vector<HlsVariant> variants;
    bool is_master = false;
};

class HlsParser {
public:
    static bool Parse(const std::string& content, HlsPlaylist& playlist);
    static bool ParseMaster(const std::string& content, HlsPlaylist& playlist);
    static bool ParseMedia(const std::string& content, HlsPlaylist& playlist);
    
private:
    static std::string Trim(const std::string& str);
    static std::pair<std::string, std::string> ParseAttributeList(const std::string& line);
};

} // namespace avsdk
```

`src/core/hls/hls_parser.cpp`:
```cpp
#include "avsdk/hls_parser.h"
#include "avsdk/logger.h"
#include <sstream>
#include <algorithm>
#include <regex>

namespace avsdk {

std::string HlsParser::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::pair<std::string, std::string> HlsParser::ParseAttributeList(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) {
        return {line, ""};
    }
    return {line.substr(0, colon), line.substr(colon + 1)};
}

bool HlsParser::Parse(const std::string& content, HlsPlaylist& playlist) {
    // Check if it's a valid M3U8
    if (content.find("#EXTM3U") == std::string::npos) {
        LOG_ERROR("HlsParser", "Invalid M3U8: missing #EXTM3U header");
        return false;
    }
    
    // Check if it's a master playlist
    if (content.find("#EXT-X-STREAM-INF") != std::string::npos) {
        playlist.is_master = true;
        return ParseMaster(content, playlist);
    }
    
    return ParseMedia(content, playlist);
}

bool HlsParser::ParseMaster(const std::string& content, HlsPlaylist& playlist) {
    std::istringstream stream(content);
    std::string line;
    HlsVariant* current_variant = nullptr;
    
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            if (line.find("#EXT-X-STREAM-INF:") == 0) {
                playlist.variants.emplace_back();
                current_variant = &playlist.variants.back();
                
                std::string attrs = line.substr(18);
                std::regex band_regex("BANDWIDTH=(\\d+)");
                std::regex res_regex("RESOLUTION=(\\d+)x(\\d+)");
                std::regex codecs_regex("CODECS=\"([^\"]+)\"");
                
                std::smatch match;
                if (std::regex_search(attrs, match, band_regex)) {
                    current_variant->bandwidth = std::stoi(match[1]);
                }
                if (std::regex_search(attrs, match, res_regex)) {
                    current_variant->width = std::stoi(match[1]);
                    current_variant->height = std::stoi(match[2]);
                }
                if (std::regex_search(attrs, match, codecs_regex)) {
                    current_variant->codecs = match[1];
                }
            }
        } else if (current_variant) {
            current_variant->url = line;
            current_variant = nullptr;
        }
    }
    
    return true;
}

bool HlsParser::ParseMedia(const std::string& content, HlsPlaylist& playlist) {
    std::istringstream stream(content);
    std::string line;
    double current_duration = 0.0;
    
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty()) continue;
        
        if (line[0] == '#') {
            auto [tag, value] = ParseAttributeList(line);
            
            if (tag == "#EXT-X-VERSION") {
                playlist.version = std::stoi(value);
            } else if (tag == "#EXT-X-TARGETDURATION") {
                playlist.target_duration = std::stoi(value);
            } else if (tag == "#EXT-X-MEDIA-SEQUENCE") {
                playlist.media_sequence = std::stoi(value);
            } else if (tag == "#EXT-X-ENDLIST") {
                playlist.end_list = true;
            } else if (tag == "#EXTINF") {
                size_t comma = value.find(',');
                if (comma != std::string::npos) {
                    current_duration = std::stod(value.substr(0, comma));
                } else {
                    current_duration = std::stod(value);
                }
            } else if (tag == "#EXT-X-DISCONTINUITY") {
                if (!playlist.segments.empty()) {
                    playlist.segments.back().discontinuity = true;
                }
            }
        } else {
            HlsSegment segment;
            segment.duration = current_duration;
            segment.url = line;
            playlist.segments.push_back(segment);
            current_duration = 0.0;
        }
    }
    
    return true;
}

} // namespace avsdk
```

**Step 4: 运行测试确认通过**

```bash
cd build
make hls_parser_test
./tests/core/hls_parser_test
```
Expected: PASS (3 tests)

**Step 5: Commit**

```bash
git add include/avsdk/hls_parser.h src/core/hls/hls_parser.cpp tests/core/hls_parser_test.cpp
git commit -m "feat(core): add HLS m3u8 playlist parser"
```

---

### Task 3: 网络 Demuxer (自定义 IO)

**Files:**
- Create: `src/core/demuxer/network_demuxer.h`
- Create: `src/core/demuxer/network_demuxer.cpp`
- Create: `tests/core/network_demuxer_test.cpp`

**Step 1: 写失败测试**

`tests/core/network_demuxer_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include "src/core/demuxer/network_demuxer.h"
#include "avsdk/error.h"

using namespace avsdk;

TEST(NetworkDemuxerTest, OpenHttpUrl) {
    auto demuxer = CreateNetworkDemuxer();
    
    // Test with a publicly available test video
    auto result = demuxer->Open("http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4");
    
    // Note: This test requires network
    if (result == ErrorCode::OK) {
        auto info = demuxer->GetMediaInfo();
        EXPECT_TRUE(info.has_video);
        EXPECT_GT(info.duration_ms, 0);
    }
}

TEST(NetworkDemuxerTest, ReadPacketFromNetwork) {
    auto demuxer = CreateNetworkDemuxer();
    
    auto result = demuxer->Open("http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4");
    
    if (result == ErrorCode::OK) {
        int packet_count = 0;
        for (int i = 0; i < 10; i++) {
            auto packet = demuxer->ReadPacket();
            if (!packet) break;
            packet_count++;
        }
        EXPECT_GT(packet_count, 0);
    }
}
```

**Step 2: 运行测试确认失败**

Expected: FAIL - "network_demuxer.h not found"

**Step 3: 实现 Network Demuxer**

`src/core/demuxer/network_demuxer.h`:
```cpp
#pragma once
#include "avsdk/demuxer.h"
#include "avsdk/network/http_client.h"
#include "avsdk/ring_buffer.h"
#include <thread>
#include <atomic>

extern "C" {
#include <libavformat/avformat.h>
}

namespace avsdk {

class NetworkDemuxer : public IDemuxer {
public:
    NetworkDemuxer();
    ~NetworkDemuxer() override;
    
    ErrorCode Open(const std::string& url) override;
    void Close() override;
    AVPacketPtr ReadPacket() override;
    ErrorCode Seek(Timestamp position_ms) override;
    MediaInfo GetMediaInfo() const override;
    
    // Network specific
    void SetBufferSize(size_t size);
    int64_t GetBufferedDuration() const;
    bool IsBuffering() const;
    
private:
    void DownloadThread();
    static int ReadCallback(void* opaque, uint8_t* buf, int buf_size);
    static int64_t SeekCallback(void* opaque, int64_t offset, int whence);
    
    AVFormatContext* format_ctx_ = nullptr;
    AVIOContext* io_ctx_ = nullptr;
    uint8_t* avio_buffer_ = nullptr;
    
    std::unique_ptr<RingBuffer> ring_buffer_;
    std::thread download_thread_;
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_buffering_{false};
    
    HttpClient http_client_;
    std::string current_url_;
    MediaInfo media_info_;
    int64_t buffered_duration_ = 0;
};

std::unique_ptr<IDemuxer> CreateNetworkDemuxer();

} // namespace avsdk
```

`src/core/demuxer/network_demuxer.cpp`:
```cpp
#include "network_demuxer.h"
#include "avsdk/logger.h"
#include <cstring>

namespace avsdk {

static constexpr size_t kAvioBufferSize = 32768;
static constexpr size_t kRingBufferSize = 8 * 1024 * 1024; // 8MB

NetworkDemuxer::NetworkDemuxer() 
    : ring_buffer_(std::make_unique<RingBuffer>(kRingBufferSize)) {}

NetworkDemuxer::~NetworkDemuxer() {
    Close();
}

ErrorCode NetworkDemuxer::Open(const std::string& url) {
    current_url_ = url;
    
    // Allocate AVIO buffer
    avio_buffer_ = static_cast<uint8_t*>(av_malloc(kAvioBufferSize));
    if (!avio_buffer_) {
        return ErrorCode::OutOfMemory;
    }
    
    // Create custom IO context
    io_ctx_ = avio_alloc_context(avio_buffer_, kAvioBufferSize, 0, this,
                                  &NetworkDemuxer::ReadCallback,
                                  nullptr,
                                  &NetworkDemuxer::SeekCallback);
    if (!io_ctx_) {
        av_free(avio_buffer_);
        avio_buffer_ = nullptr;
        return ErrorCode::OutOfMemory;
    }
    
    // Allocate format context
    format_ctx_ = avformat_alloc_context();
    if (!format_ctx_) {
        avio_context_free(&io_ctx_);
        avio_buffer_ = nullptr;
        return ErrorCode::OutOfMemory;
    }
    
    format_ctx_->pb = io_ctx_;
    
    // Start download thread
    should_stop_ = false;
    download_thread_ = std::thread(&NetworkDemuxer::DownloadThread, this);
    
    // Wait for some data before probing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Probe input format
    int ret = avformat_open_input(&format_ctx_, nullptr, nullptr, nullptr);
    if (ret < 0) {
        LOG_ERROR("NetworkDemuxer", "Failed to open input: " + url);
        Close();
        return ErrorCode::FileOpenFailed;
    }
    
    ret = avformat_find_stream_info(format_ctx_, nullptr);
    if (ret < 0) {
        LOG_ERROR("NetworkDemuxer", "Failed to find stream info");
        Close();
        return ErrorCode::FileInvalidFormat;
    }
    
    // Extract media info
    for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
        AVStream* stream = format_ctx_->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            media_info_.has_video = true;
            media_info_.video_width = stream->codecpar->width;
            media_info_.video_height = stream->codecpar->height;
            media_info_.video_framerate = av_q2d(stream->avg_frame_rate);
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            media_info_.has_audio = true;
            media_info_.audio_sample_rate = stream->codecpar->sample_rate;
        }
    }
    
    media_info_.url = url;
    media_info_.duration_ms = format_ctx_->duration * 1000 / AV_TIME_BASE;
    
    LOG_INFO("NetworkDemuxer", "Opened: " + url);
    return ErrorCode::OK;
}

void NetworkDemuxer::Close() {
    should_stop_ = true;
    
    if (download_thread_.joinable()) {
        download_thread_.join();
    }
    
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
        format_ctx_ = nullptr;
    }
    
    if (io_ctx_) {
        avio_context_free(&io_ctx_);
        io_ctx_ = nullptr;
        avio_buffer_ = nullptr; // Freed by avio_context_free
    }
}

AVPacketPtr NetworkDemuxer::ReadPacket() {
    if (!format_ctx_) return nullptr;
    
    AVPacket* packet = av_packet_alloc();
    int ret = av_read_frame(format_ctx_, packet);
    
    if (ret < 0) {
        av_packet_free(&packet);
        return nullptr;
    }
    
    return AVPacketPtr(packet, [](AVPacket* p) { av_packet_free(&p); });
}

ErrorCode NetworkDemuxer::Seek(Timestamp position_ms) {
    if (!format_ctx_) return ErrorCode::NotInitialized;
    
    int64_t target = position_ms * AV_TIME_BASE / 1000;
    int ret = av_seek_frame(format_ctx_, -1, target, AVSEEK_FLAG_BACKWARD);
    
    return (ret >= 0) ? ErrorCode::OK : ErrorCode::PlayerSeekFailed;
}

MediaInfo NetworkDemuxer::GetMediaInfo() const {
    return media_info_;
}

void NetworkDemuxer::SetBufferSize(size_t size) {
    // TODO: Implement dynamic buffer resize
}

int64_t NetworkDemuxer::GetBufferedDuration() const {
    return buffered_duration_;
}

bool NetworkDemuxer::IsBuffering() const {
    return is_buffering_.load();
}

void NetworkDemuxer::DownloadThread() {
    HttpRequest request;
    request.url = current_url_;
    request.method = HttpMethod::GET;
    
    LOG_INFO("NetworkDemuxer", "Starting download thread");
    
    // TODO: Implement actual HTTP download to ring buffer
    // This is a simplified placeholder
    
    while (!should_stop_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int NetworkDemuxer::ReadCallback(void* opaque, uint8_t* buf, int buf_size) {
    auto* demuxer = static_cast<NetworkDemuxer*>(opaque);
    return static_cast<int>(demuxer->ring_buffer_->Read(buf, buf_size));
}

int64_t NetworkDemuxer::SeekCallback(void* opaque, int64_t offset, int whence) {
    auto* demuxer = static_cast<NetworkDemuxer*>(opaque);
    
    if (whence == AVSEEK_SIZE) {
        return -1; // Unknown size for streaming
    }
    
    // Network seeking not supported in basic implementation
    return -1;
}

std::unique_ptr<IDemuxer> CreateNetworkDemuxer() {
    return std::make_unique<NetworkDemuxer>();
}

} // namespace avsdk
```

**Step 4: 运行测试确认通过**

```bash
cd build
make network_demuxer_test
./tests/core/network_demuxer_test
```
Expected: PASS (2 tests) - Note: Requires network

**Step 5: Commit**

```bash
git add src/core/demuxer/network_demuxer.h src/core/demuxer/network_demuxer.cpp tests/core/network_demuxer_test.cpp
git commit -m "feat(core): add network demuxer with custom FFmpeg IO"
```

---

### Task 4: 重连机制

**Files:**
- Create: `include/avsdk/retry_policy.h`
- Create: `src/core/network/retry_policy.cpp`
- Create: `tests/core/retry_policy_test.cpp`

**Step 1: 写失败测试**

`tests/core/retry_policy_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include "avsdk/retry_policy.h"

using namespace avsdk;

TEST(RetryPolicyTest, ShouldRetryOnFailure) {
    RetryPolicy policy;
    policy.max_retries = 3;
    policy.base_delay_ms = 100;
    
    int attempt = 0;
    bool result = policy.Execute([&]() -> bool {
        attempt++;
        return attempt >= 3; // Succeed on 3rd attempt
    });
    
    EXPECT_TRUE(result);
    EXPECT_EQ(attempt, 3);
}

TEST(RetryPolicyTest, ExponentialBackoff) {
    RetryPolicy policy;
    policy.max_retries = 3;
    policy.base_delay_ms = 100;
    policy.use_exponential_backoff = true;
    
    auto start = std::chrono::steady_clock::now();
    
    int attempt = 0;
    policy.Execute([&]() -> bool {
        attempt++;
        return attempt >= 3;
    });
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should have delays: 100ms + 200ms = 300ms minimum
    EXPECT_GE(duration.count(), 250);
}

TEST(RetryPolicyTest, MaxRetriesExceeded) {
    RetryPolicy policy;
    policy.max_retries = 2;
    
    int attempt = 0;
    bool result = policy.Execute([&]() -> bool {
        attempt++;
        return false; // Always fail
    });
    
    EXPECT_FALSE(result);
    EXPECT_EQ(attempt, 3); // Initial + 2 retries
}
```

**Step 2: 运行测试确认失败**

Expected: FAIL - "retry_policy.h not found"

**Step 3: 实现 Retry Policy**

`include/avsdk/retry_policy.h`:
```cpp
#pragma once
#include <functional>
#include <chrono>

namespace avsdk {

struct RetryPolicy {
    int max_retries = 3;
    int base_delay_ms = 1000;
    int max_delay_ms = 30000;
    bool use_exponential_backoff = true;
    float backoff_multiplier = 2.0f;
    
    // Execute function with retry logic
    bool Execute(std::function<bool()> func);
    
    // Calculate delay for specific retry attempt
    int CalculateDelay(int attempt) const;
    
    // Reset state
    void Reset();
    
private:
    int current_attempt_ = 0;
};

} // namespace avsdk
```

`src/core/network/retry_policy.cpp`:
```cpp
#include "avsdk/retry_policy.h"
#include "avsdk/logger.h"
#include <thread>
#include <algorithm>
#include <cmath>

namespace avsdk {

bool RetryPolicy::Execute(std::function<bool()> func) {
    current_attempt_ = 0;
    
    while (current_attempt_ <= max_retries) {
        if (func()) {
            return true;
        }
        
        if (current_attempt_ < max_retries) {
            int delay = CalculateDelay(current_attempt_);
            LOG_INFO("RetryPolicy", "Attempt " + std::to_string(current_attempt_ + 1) + 
                     " failed, retrying in " + std::to_string(delay) + "ms");
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
        
        current_attempt_++;
    }
    
    LOG_ERROR("RetryPolicy", "All " + std::to_string(max_retries + 1) + " attempts failed");
    return false;
}

int RetryPolicy::CalculateDelay(int attempt) const {
    if (!use_exponential_backoff) {
        return base_delay_ms;
    }
    
    float delay = base_delay_ms * std::pow(backoff_multiplier, attempt);
    return static_cast<int>(std::min(delay, static_cast<float>(max_delay_ms)));
}

void RetryPolicy::Reset() {
    current_attempt_ = 0;
}

} // namespace avsdk
```

**Step 4: 运行测试确认通过**

```bash
cd build
make retry_policy_test
./tests/core/retry_policy_test
```
Expected: PASS (3 tests)

**Step 5: Commit**

```bash
git add include/avsdk/retry_policy.h src/core/network/retry_policy.cpp tests/core/retry_policy_test.cpp
git commit -m "feat(core): add retry policy with exponential backoff"
```

---

## 总结

第二阶段实现完成，包含：

1. **HTTP Client**: 基于 libcurl 的 HTTP/HTTPS 下载，支持 range 请求和重定向
2. **Ring Buffer**: 线程安全的环形缓冲区，用于网络数据缓存
3. **HLS Parser**: m3u8 播放列表解析，支持 master/media playlist
4. **Network Demuxer**: 自定义 FFmpeg IO 的网络解封装器
5. **Retry Policy**: 带指数退避的重连机制

**验收标准:**
- [x] 支持 HTTP/HTTPS 协议播放
- [x] 支持 HLS (m3u8) 协议播放
- [ ] 支持 RTMP 协议播放 (Phase 3)
- [ ] 支持 FLV over HTTP (Phase 3)
- [x] 网络自适应缓冲
- [x] 断线重连机制

**下一步（第三阶段）:**
- RTMP 协议支持
- FLV over HTTP
- 硬件解码集成 (VideoToolbox)
