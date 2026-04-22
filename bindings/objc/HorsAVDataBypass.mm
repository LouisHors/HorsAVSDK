#import "HorsAVDataBypass.h"
#import "HorsAVPlayer.h"

// MARK: - HorsAVDataBypassConfiguration Implementation

@implementation HorsAVDataBypassConfiguration

- (instancetype)init {
    self = [super init];
    if (self) {
        [self setDefaults];
    }
    return self;
}

- (void)setDefaults {
    _enableVideoFrames = NO;
    _enableAudioFrames = NO;
    _enableEncodedData = NO;
    _bypassStage = HorsAVDataBypassStageDecoded;
    _maxCallbackFrequency = 30;
}

- (id)copyWithZone:(NSZone *)zone {
    HorsAVDataBypassConfiguration *copy = [[self class] allocWithZone:zone];
    copy.enableVideoFrames = self.enableVideoFrames;
    copy.enableAudioFrames = self.enableAudioFrames;
    copy.enableEncodedData = self.enableEncodedData;
    copy.bypassStage = self.bypassStage;
    copy.maxCallbackFrequency = self.maxCallbackFrequency;
    return copy;
}

+ (instancetype)defaultConfiguration {
    return [[self alloc] init];
}

@end
