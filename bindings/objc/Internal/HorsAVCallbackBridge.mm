#import "HorsAVCallbackBridge.h"
#import "HorsAVPlayer.h"
#import "HorsAVTypes.h"
#import "HorsAVError.h"

#include "avsdk/player_callback.h"
#include "avsdk/types.h"
#include <memory>

using namespace avsdk;

// MARK: - C++ Callback Implementation

namespace HorsAV {

class ObjCPlayerCallback : public IPlayerCallback {
public:
    ObjCPlayerCallback(HorsAVPlayer *player) : _player(player) {}

    void OnPrepared(const MediaInfo& info) override {
        // Delegate is notified by the player itself after open
    }

    void OnStateChanged(PlayerState oldState, PlayerState newState) override {
        // State changes are handled by the player
    }

    void OnError(ErrorCode error, const std::string& message) override {
        HorsAVPlayer *player = _player;
        if (!player) return;

        NSError *nsError = [NSError errorWithDomain:HorsAVErrorDomain
                                               code:static_cast<HorsAVErrorCode>(error)
                                           userInfo:@{NSLocalizedDescriptionKey: @(message.c_str())}];

        dispatch_async(dispatch_get_main_queue(), ^{
            if ([player.delegate respondsToSelector:@selector(player:didEncounterError:)]) {
                [player.delegate player:player didEncounterError:nsError];
            }
        });
    }

    void OnComplete() override {
        HorsAVPlayer *player = _player;
        if (!player) return;

        dispatch_async(dispatch_get_main_queue(), ^{
            if ([player.delegate respondsToSelector:@selector(playerDidCompletePlayback:)]) {
                [player.delegate playerDidCompletePlayback:player];
            }
        });
    }

    void OnProgress(Timestamp currentPosition, Timestamp duration) override {
        HorsAVPlayer *player = _player;
        if (!player) return;

        NSTimeInterval current = currentPosition / 1000.0;
        NSTimeInterval total = duration / 1000.0;

        dispatch_async(dispatch_get_main_queue(), ^{
            player.currentTime = current;

            if ([player.delegate respondsToSelector:@selector(player:didUpdateProgress:duration:)]) {
                [player.delegate player:player didUpdateProgress:current duration:total];
            }
        });
    }

    void OnBuffering(bool isBuffering, int percent) override {
        HorsAVPlayer *player = _player;
        if (!player) return;

        dispatch_async(dispatch_get_main_queue(), ^{
            player.buffering = isBuffering;
            player.bufferingPercent = percent;

            if ([player.delegate respondsToSelector:@selector(player:didChangeBuffering:percent:)]) {
                [player.delegate player:player didChangeBuffering:isBuffering percent:percent];
            }
        });
    }

    void OnSeekComplete(Timestamp position) override {
        HorsAVPlayer *player = _player;
        if (!player) return;

        NSTimeInterval pos = position / 1000.0;

        dispatch_async(dispatch_get_main_queue(), ^{
            if ([player.delegate respondsToSelector:@selector(player:didCompleteSeekTo:)]) {
                [player.delegate player:player didCompleteSeekTo:pos];
            }
        });
    }

private:
    __weak HorsAVPlayer *_player;
};

} // namespace HorsAVAV

// MARK: - Objective-C Implementation

@interface HorsAVCallbackBridge () {
    std::shared_ptr<HorsAV::ObjCPlayerCallback> _callback;
}
@end

@implementation HorsAVCallbackBridge

- (instancetype)initWithPlayer:(HorsAVPlayer *)player {
    self = [super init];
    if (self) {
        _callback = std::make_shared<HorsAV::ObjCPlayerCallback>(player);
    }
    return self;
}

- (std::shared_ptr<avsdk::IPlayerCallback>)playerCallback {
    return _callback;
}

@end
