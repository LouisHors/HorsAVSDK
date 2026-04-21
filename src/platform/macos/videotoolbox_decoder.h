#pragma once
#include "avsdk/decoder.h"
#include <VideoToolbox/VideoToolbox.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace avsdk {

class VideoToolboxDecoder : public IDecoder {
public:
    VideoToolboxDecoder();
    ~VideoToolboxDecoder() override;

    ErrorCode Initialize(AVCodecParameters* codecpar) override;
    AVFramePtr Decode(const AVPacketPtr& packet) override;
    void Flush() override;
    void Close() override;

    // VideoToolbox specific
    CVPixelBufferRef GetPixelBuffer() const { return pixel_buffer_; }
    bool IsHardwareAccelerated() const { return session_ != nullptr; }

private:
    static void OutputCallback(void* decompression_output_ref_con,
                               void* source_frame_ref_con,
                               OSStatus status,
                               VTDecodeInfoFlags info_flags,
                               CVImageBufferRef image_buffer,
                               CMTime presentation_time_stamp,
                               CMTime presentation_duration);

    OSStatus CreateH264FormatDescription(const uint8_t* extradata, int size,
                                          int width, int height,
                                          CMVideoFormatDescriptionRef* formatDesc);
    OSStatus CreateHEVCFormatDescription(const uint8_t* extradata, int size,
                                          int width, int height,
                                          CMVideoFormatDescriptionRef* formatDesc);
    OSStatus CreateDecompressionSession();

    VTDecompressionSessionRef session_ = nullptr;
    CMVideoFormatDescriptionRef format_desc_ = nullptr;
    CVPixelBufferRef pixel_buffer_ = nullptr;

    int width_ = 0;
    int height_ = 0;
    AVCodecID codec_id_ = AV_CODEC_ID_NONE;

    std::mutex mutex_;
    std::condition_variable decode_done_;
    bool decode_completed_ = false;
    OSStatus decode_status_ = noErr;
};

std::unique_ptr<IDecoder> CreateVideoToolboxDecoder();

bool IsHardwareDecoderAvailable(AVCodecID codec_id);

} // namespace avsdk
