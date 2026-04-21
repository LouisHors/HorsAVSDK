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

    HttpResponse Execute(const HttpRequest& request);
    void DownloadAsync(const HttpRequest& request,
                       std::function<void(const uint8_t* data, size_t size)> data_callback,
                       std::function<void(int64_t downloaded, int64_t total)> progress_callback);
    void Cancel();

private:
    CURL* curl_;
    std::atomic<bool> cancelled_;
    std::function<void(const uint8_t*, size_t)> write_callback_;
};

HttpResponse HttpClient::Impl::Execute(const HttpRequest& request) {
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

void HttpClient::Impl::DownloadAsync(const HttpRequest& request,
                                     std::function<void(const uint8_t* data, size_t size)> data_callback,
                                     std::function<void(int64_t downloaded, int64_t total)> progress_callback) {
    std::thread([this, request, data_callback, progress_callback]() {
        if (!curl_) {
            LOG_ERROR("HttpClient", "CURL not initialized");
            return;
        }

        cancelled_ = false;

        // Reset curl options for this request
        curl_easy_reset(curl_);

        curl_easy_setopt(curl_, CURLOPT_URL, request.url.c_str());
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, request.follow_redirects ? 1L : 0L);
        curl_easy_setopt(curl_, CURLOPT_MAXREDIRS, request.max_redirects);
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, request.timeout_ms);
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, const_cast<std::function<void(const uint8_t*, size_t)>*>(&data_callback));

        // Range request
        if (request.range_start >= 0) {
            std::string range;
            if (request.range_end > request.range_start) {
                range = std::to_string(request.range_start) + "-" + std::to_string(request.range_end);
            } else {
                range = std::to_string(request.range_start) + "-";
            }
            curl_easy_setopt(curl_, CURLOPT_RANGE, range.c_str());
        }

        // Progress callback
        if (progress_callback) {
            curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl_, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
            curl_easy_setopt(curl_, CURLOPT_XFERINFODATA, const_cast<std::function<void(int64_t, int64_t)>*>(&progress_callback));
        }

        CURLcode res = curl_easy_perform(curl_);

        if (res != CURLE_OK && res != CURLE_ABORTED_BY_CALLBACK) {
            LOG_ERROR("HttpClient", "Download failed: " + std::string(curl_easy_strerror(res)));
        }
    }).detach();
}

void HttpClient::Impl::Cancel() {
    cancelled_ = true;
}

HttpClient::HttpClient() : impl_(std::make_unique<Impl>()) {}

HttpClient::~HttpClient() = default;

HttpResponse HttpClient::Execute(const HttpRequest& request) {
    return impl_->Execute(request);
}

void HttpClient::DownloadAsync(const HttpRequest& request,
                               std::function<void(const uint8_t* data, size_t size)> data_callback,
                               std::function<void(int64_t downloaded, int64_t total)> progress_callback) {
    impl_->DownloadAsync(request, data_callback, progress_callback);
}

int HttpClient::ProgressCallback(void* clientp, int64_t dltotal, int64_t dlnow,
                                 int64_t ultotal, int64_t ulnow) {
    auto* callback = static_cast<std::function<void(int64_t, int64_t)>*>(clientp);
    if (callback && *callback) {
        (*callback)(static_cast<int64_t>(dlnow), static_cast<int64_t>(dltotal));
    }
    return 0;
}

void HttpClient::Cancel() {
    impl_->Cancel();
}

} // namespace avsdk
