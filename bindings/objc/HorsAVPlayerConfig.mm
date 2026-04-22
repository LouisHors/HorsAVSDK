#import "HorsAVPlayerConfig.h"

// MARK: - Implementation

@implementation HorsAVPlayerConfiguration

- (instancetype)init {
    self = [super init];
    if (self) {
        [self setDefaults];
    }
    return self;
}

- (void)setDefaults {
    _decoderMode = HorsAVDecoderModeHardwareFirst;
    _enableHardwareDecoder = YES;
    _bufferTime = 1.0;
    _logLevel = @"info";
    _enableAudioResampling = YES;
    _enableAVSync = YES;
    _maxSyncDriftMs = 40;
}

- (id)copyWithZone:(NSZone *)zone {
    HorsAVPlayerConfiguration *copy = [[self class] allocWithZone:zone];
    copy.decoderMode = self.decoderMode;
    copy.enableHardwareDecoder = self.enableHardwareDecoder;
    copy.bufferTime = self.bufferTime;
    copy.logLevel = [self.logLevel copy];
    copy.enableAudioResampling = self.enableAudioResampling;
    copy.enableAVSync = self.enableAVSync;
    copy.maxSyncDriftMs = self.maxSyncDriftMs;
    return copy;
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return [self copyWithZone:zone];
}

+ (instancetype)defaultConfiguration {
    return [[self alloc] init];
}

+ (instancetype)lowLatencyConfiguration {
    HorsAVPlayerConfiguration *config = [[self alloc] init];
    config.bufferTime = 0.3;
    config.maxSyncDriftMs = 20;
    return config;
}

+ (instancetype)smoothPlaybackConfiguration {
    HorsAVPlayerConfiguration *config = [[self alloc] init];
    config.bufferTime = 3.0;
    config.maxSyncDriftMs = 60;
    return config;
}

@end
