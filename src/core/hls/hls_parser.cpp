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
            auto tag_value = ParseAttributeList(line);
            const std::string& tag = tag_value.first;
            const std::string& value = tag_value.second;

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
