#include "avsdk/data_bypass.h"

namespace avsdk {

void DataBypassManager::RegisterBypass(std::shared_ptr<IDataBypass> bypass) {
    if (!bypass) return;

    std::lock_guard<std::mutex> lock(mutex_);
    // Check if already registered
    auto it = std::find_if(bypasses_.begin(), bypasses_.end(),
        [&bypass](const std::weak_ptr<IDataBypass>& wp) {
            auto sp = wp.lock();
            return sp && sp == bypass;
        });

    if (it == bypasses_.end()) {
        bypasses_.push_back(bypass);
    }
}

void DataBypassManager::UnregisterBypass(std::shared_ptr<IDataBypass> bypass) {
    if (!bypass) return;

    std::lock_guard<std::mutex> lock(mutex_);
    bypasses_.erase(
        std::remove_if(bypasses_.begin(), bypasses_.end(),
            [&bypass](const std::weak_ptr<IDataBypass>& wp) {
                auto sp = wp.lock();
                return !sp || sp == bypass;
            }),
        bypasses_.end()
    );
}

void DataBypassManager::ClearBypasses() {
    std::lock_guard<std::mutex> lock(mutex_);
    bypasses_.clear();
}

std::vector<std::shared_ptr<IDataBypass>> DataBypassManager::GetValidBypasses() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<IDataBypass>> valid;

    // Remove expired weak_ptr and collect valid ones
    bypasses_.erase(
        std::remove_if(bypasses_.begin(), bypasses_.end(),
            [&valid](const std::weak_ptr<IDataBypass>& wp) {
                auto sp = wp.lock();
                if (sp) {
                    valid.push_back(sp);
                    return false;  // Keep valid entry
                }
                return true;  // Remove expired entry
            }),
        bypasses_.end()
    );

    return valid;
}

void DataBypassManager::DispatchRawPacket(const EncodedPacket& packet) {
    auto bypasses = GetValidBypasses();
    for (auto& bypass : bypasses) {
        if (bypass) {
            bypass->OnRawPacket(packet);
        }
    }
}

void DataBypassManager::DispatchDecodedVideoFrame(const VideoFrame& frame) {
    auto bypasses = GetValidBypasses();
    for (auto& bypass : bypasses) {
        if (bypass) {
            bypass->OnDecodedVideoFrame(frame);
        }
    }
}

void DataBypassManager::DispatchDecodedAudioFrame(const AudioFrame& frame) {
    auto bypasses = GetValidBypasses();
    for (auto& bypass : bypasses) {
        if (bypass) {
            bypass->OnDecodedAudioFrame(frame);
        }
    }
}

void DataBypassManager::DispatchPreRenderVideoFrame(VideoFrame& frame) {
    auto bypasses = GetValidBypasses();
    for (auto& bypass : bypasses) {
        if (bypass) {
            bypass->OnPreRenderVideoFrame(frame);
        }
    }
}

void DataBypassManager::DispatchPreRenderAudioFrame(AudioFrame& frame) {
    auto bypasses = GetValidBypasses();
    for (auto& bypass : bypasses) {
        if (bypass) {
            bypass->OnPreRenderAudioFrame(frame);
        }
    }
}

void DataBypassManager::DispatchEncodedPacket(const EncodedPacket& packet) {
    auto bypasses = GetValidBypasses();
    for (auto& bypass : bypasses) {
        if (bypass) {
            bypass->OnEncodedPacket(packet);
        }
    }
}

size_t DataBypassManager::GetBypassCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Count valid entries only (cannot erase in const method)
    size_t count = 0;
    for (const auto& wp : bypasses_) {
        if (!wp.expired()) {
            ++count;
        }
    }
    return count;
}

} // namespace avsdk
