#pragma once
#include <string>
#include "avsdk/types.h"
#include "avsdk/error.h"

namespace avsdk {

// Player state change callback
class IPlayerCallback {
public:
    virtual ~IPlayerCallback() = default;

    // Called when player is prepared and ready to play
    virtual void OnPrepared(const MediaInfo& info) = 0;

    // Called when player state changes
    virtual void OnStateChanged(PlayerState oldState, PlayerState newState) = 0;

    // Called when an error occurs
    virtual void OnError(ErrorCode error, const std::string& message) = 0;

    // Called when playback completes (reaches end of stream)
    virtual void OnComplete() = 0;

    // Called periodically with playback progress (milliseconds)
    virtual void OnProgress(Timestamp currentPosition, Timestamp duration) = 0;

    // Called when buffering state changes
    virtual void OnBuffering(bool isBuffering, int percent) = 0;

    // Called when seeking completes
    virtual void OnSeekComplete(Timestamp position) = 0;
};

} // namespace avsdk
