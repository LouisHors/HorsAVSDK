#include <iostream>
#include "avsdk/player.h"
#include "avsdk/player_config.h"
#include "avsdk/error.h"

using namespace avsdk;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <video_file>" << std::endl;
        return 1;
    }

    std::string video_file = argv[1];

    std::cout << "HorsAVSDK Simple Player" << std::endl;
    std::cout << "======================" << std::endl;

    auto player = CreatePlayer();
    if (!player) {
        std::cerr << "Failed to create player" << std::endl;
        return 1;
    }

    PlayerConfig config;
    if (player->Initialize(config) != ErrorCode::OK) {
        std::cerr << "Failed to initialize player" << std::endl;
        return 1;
    }

    if (player->Open(video_file) != ErrorCode::OK) {
        std::cerr << "Failed to open: " << video_file << std::endl;
        return 1;
    }

    auto info = player->GetMediaInfo();
    std::cout << "File: " << video_file << std::endl;
    std::cout << "Duration: " << info.duration_ms / 1000.0 << "s" << std::endl;
    std::cout << "Resolution: " << info.video_width << "x" << info.video_height << std::endl;
    std::cout << std::endl;

    std::cout << "Playing... (Press Enter to stop)" << std::endl;
    player->Play();

    std::cin.get();

    player->Stop();
    std::cout << "Playback stopped" << std::endl;

    return 0;
}
