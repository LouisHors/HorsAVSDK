#pragma once
#include "avsdk/decoder.h"
#include <VideoToolbox/VideoToolbox.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <condition_variable>
#include <mutex>

namespace avsdk {

class VideoToolboxDecoder : public IDecoder {
public:
    VideoToolboxDecoder();
    ~VideoToolboxDecoder() override;

    ErrorCode Initialize(CodecType codec_type, int width, int height) override;
    AVFramePtr Decode(const AVPacketPtr& packet) override;
    void Flush() override;
    void Close() override;

    // VideoToolbox specific
    CVPixelBufferRef GetPixelBuffer() const { return pixel_buffer_; }

private:
    static void OutputCallback(void* decompression_output_ref_con,
                               void* source_frame_ref_con,
                               OSStatus status,
                               VTDecodeInfoFlags info_flags,
                               CVImageBufferRef image_buffer,
                               CMTime presentation_time_stamp,
                               CMTime presentation_duration);

    VTDecompressionSessionRef session_ = nullptr;
    CMVideoFormatDescriptionRef format_desc_ = nullptr;
    CVPixelBufferRef pixel_buffer_ = nullptr;

    CodecType codec_type_ = CodecType::H264;
    int width_ = 0;
    int height_ = 0;

    std::mutex mutex_;
    std::condition_variable decode_done_;
    bool decode_completed_ = false;
    OSStatus decode_status_ = noErr;
};

std::unique_ptr<IDecoder> CreateVideoToolboxDecoder();

bool IsHardwareDecoderAvailable(CodecType codec_type);

} // namespace avsdk
