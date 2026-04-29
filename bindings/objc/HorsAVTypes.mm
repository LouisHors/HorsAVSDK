#import "HorsAVTypes.h"

// MARK: - HorsAVMediaInfo Implementation

@interface HorsAVAudioTrackInfo ()
- (instancetype)initWithTrackIndex:(NSInteger)trackIndex
                          language:(NSString *)language
                             title:(NSString *)title
                        sampleRate:(NSInteger)sampleRate
                          channels:(NSInteger)channels NS_DESIGNATED_INITIALIZER;
@end

@implementation HorsAVAudioTrackInfo

- (instancetype)initWithTrackIndex:(NSInteger)trackIndex
                          language:(NSString *)language
                             title:(NSString *)title
                        sampleRate:(NSInteger)sampleRate
                          channels:(NSInteger)channels {
    self = [super init];
    if (self) {
        _trackIndex = trackIndex;
        _language = [language copy];
        _title = [title copy];
        _sampleRate = sampleRate;
        _channels = channels;
    }
    return self;
}

- (instancetype)init {
    return nil;
}

- (id)copyWithZone:(NSZone *)zone {
    return [[HorsAVAudioTrackInfo allocWithZone:zone] initWithTrackIndex:self.trackIndex
                                                                language:self.language
                                                                   title:self.title
                                                              sampleRate:self.sampleRate
                                                                channels:self.channels];
}

@end

@interface HorsAVMediaInfo ()
- (instancetype)initWithURL:(NSString *)url
                     format:(NSString *)format
                   duration:(NSTimeInterval)duration
                 videoWidth:(NSInteger)width
                videoHeight:(NSInteger)height
            videoFrameRate:(double)frameRate
           audioSampleRate:(NSInteger)sampleRate
              audioChannels:(NSInteger)channels
                   hasVideo:(BOOL)hasVideo
                   hasAudio:(BOOL)hasAudio
                 audioTracks:(NSArray<HorsAVAudioTrackInfo *> *)audioTracks NS_DESIGNATED_INITIALIZER;
@end

@implementation HorsAVMediaInfo

- (instancetype)initWithURL:(NSString *)url
                     format:(NSString *)format
                   duration:(NSTimeInterval)duration
                 videoWidth:(NSInteger)width
                videoHeight:(NSInteger)height
            videoFrameRate:(double)frameRate
           audioSampleRate:(NSInteger)sampleRate
              audioChannels:(NSInteger)channels
                   hasVideo:(BOOL)hasVideo
                   hasAudio:(BOOL)hasAudio
                 audioTracks:(NSArray<HorsAVAudioTrackInfo *> *)audioTracks {
    self = [super init];
    if (self) {
        _url = [url copy];
        _format = [format copy];
        _duration = duration;
        _videoWidth = width;
        _videoHeight = height;
        _videoFrameRate = frameRate;
        _audioSampleRate = sampleRate;
        _audioChannels = channels;
        _hasVideo = hasVideo;
        _hasAudio = hasAudio;
        _audioTracks = [audioTracks copy];
    }
    return self;
}

- (instancetype)init {
    return nil; // Must use designated initializer
}

- (id)copyWithZone:(NSZone *)zone {
    return [[HorsAVMediaInfo allocWithZone:zone] initWithURL:self.url
                                                      format:self.format
                                                    duration:self.duration
                                                  videoWidth:self.videoWidth
                                                 videoHeight:self.videoHeight
                                             videoFrameRate:self.videoFrameRate
                                            audioSampleRate:self.audioSampleRate
                                               audioChannels:self.audioChannels
                                                    hasVideo:self.hasVideo
                                                    hasAudio:self.hasAudio
                                                 audioTracks:self.audioTracks];
}

@end

// MARK: - HorsAVVideoFrame Implementation

@interface HorsAVVideoFrame ()
- (instancetype)initWithSize:(CGSize)size
                   timestamp:(int64_t)timestamp
                  isKeyFrame:(BOOL)isKeyFrame
                       stage:(HorsAVDataBypassStage)stage NS_DESIGNATED_INITIALIZER;
@end

@implementation HorsAVVideoFrame

- (instancetype)initWithSize:(CGSize)size
                   timestamp:(int64_t)timestamp
                  isKeyFrame:(BOOL)isKeyFrame
                       stage:(HorsAVDataBypassStage)stage {
    self = [super init];
    if (self) {
        _size = size;
        _timestamp = timestamp;
        _isKeyFrame = isKeyFrame;
        _stage = stage;
    }
    return self;
}

- (instancetype)init {
    return nil;
}

@end

// MARK: - HorsAVAudioFrame Implementation

@interface HorsAVAudioFrame ()
- (instancetype)initWithSampleRate:(NSInteger)sampleRate
                          channels:(NSInteger)channels
                         timestamp:(int64_t)timestamp
                          samples:(NSInteger)samples
                             stage:(HorsAVDataBypassStage)stage NS_DESIGNATED_INITIALIZER;
@end

@implementation HorsAVAudioFrame

- (instancetype)initWithSampleRate:(NSInteger)sampleRate
                          channels:(NSInteger)channels
                         timestamp:(int64_t)timestamp
                          samples:(NSInteger)samples
                             stage:(HorsAVDataBypassStage)stage {
    self = [super init];
    if (self) {
        _sampleRate = sampleRate;
        _channels = channels;
        _timestamp = timestamp;
        _samples = samples;
        _stage = stage;
    }
    return self;
}

- (instancetype)init {
    return nil;
}

@end
