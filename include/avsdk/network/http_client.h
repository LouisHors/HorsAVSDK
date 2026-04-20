#pragma once
#include <string>
#include <functional>
#include <vector>
#include <memory>
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
