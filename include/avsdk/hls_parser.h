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
