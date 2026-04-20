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
    // httpbin.org may return 200 OK or 206 Partial Content depending on implementation
    EXPECT_TRUE(response.status_code == 200 || response.status_code == 206);
    EXPECT_LE(response.body.size(), 1024);
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
