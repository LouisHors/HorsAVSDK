#import "videotoolbox_decoder.h"
#include "avsdk/logger.h"
extern "C" {
#include <libavcodec/avcodec.h>
}

namespace avsdk {

VideoToolboxDecoder::VideoToolboxDecoder() = default;

VideoToolboxDecoder::~VideoToolboxDecoder() {
    Close();
}

ErrorCode VideoToolboxDecoder::Initialize(AVCodecParameters* codecpar) {
    if (!codecpar) {
        LOG_ERROR("VideoToolboxDecoder", "No codec parameters provided");
        return ErrorCode::InvalidParameter;
    }

    width_ = codecpar->width;
    height_ = codecpar->height;
    codec_id_ = codecpar->codec_id;

    // Determine codec type
    CMVideoCodecType codec_type_vt;
    switch (codecpar->codec_id) {
        case AV_CODEC_ID_H264:
            codec_type_vt = kCMVideoCodecType_H264;
            break;
        case AV_CODEC_ID_HEVC:
            codec_type_vt = kCMVideoCodecType_HEVC;
            break;
        default:
            LOG_ERROR("VideoToolboxDecoder", "Unsupported codec_id: " + std::to_string(codecpar->codec_id));
            return ErrorCode::CodecNotFound;
    }

    // For HEVC, check if codec configuration data is available
    if (codecpar->codec_id == AV_CODEC_ID_HEVC && (!codecpar->extradata || codecpar->extradata_size == 0)) {
        LOG_INFO("VideoToolboxDecoder", "HEVC requires extradata for initialization");
        // Store parameters and wait for extradata from first packet
        // For now, we'll initialize later when we have the data
    }

    // Create format description from extradata if available
    if (codecpar->extradata && codecpar->extradata_size > 0) {
        // Parse extradata to create format description
        const uint8_t* extradata = codecpar->extradata;
        int extradata_size = codecpar->extradata_size;

        // For H.264, extradata contains SPS/PPS
        // For HEVC, extradata contains VPS/SPS/PPS
        CFDataRef data = CFDataCreate(nullptr, extradata, extradata_size);
        if (!data) {
            LOG_ERROR("VideoToolboxDecoder", "Failed to create CFData from extradata");
            return ErrorCode::CodecOpenFailed;
        }

        // Try to create format description from extradata
        OSStatus status;
        if (codecpar->codec_id == AV_CODEC_ID_H264) {
            // H.264: Try to find SPS/PPS NAL units
            status = CreateH264FormatDescription(extradata, extradata_size, width_, height_, &format_desc_);
        } else {
            // HEVC: Try to find VPS/SPS/PPS NAL units
            status = CreateHEVCFormatDescription(extradata, extradata_size, width_, height_, &format_desc_);
        }

        CFRelease(data);

        if (status != noErr || !format_desc_) {
            LOG_ERROR("VideoToolboxDecoder", "Failed to create format description from extradata: " + std::to_string(status));
            // Continue without format_desc - will try to create from packets
        }
    }

    // If we have a format description, create the decompression session
    if (format_desc_) {
        OSStatus status = CreateDecompressionSession();
        if (status != noErr) {
            LOG_ERROR("VideoToolboxDecoder", "Failed to create decompression session: " + std::to_string(status));
            return ErrorCode::CodecOpenFailed;
        }
    }

    LOG_INFO("VideoToolboxDecoder", "Initialized " + std::to_string(width_) + "x" + std::to_string(height_) +
             " decoder, format_desc: " + (format_desc_ ? "yes" : "no"));
    return ErrorCode::OK;
}

OSStatus VideoToolboxDecoder::CreateH264FormatDescription(const uint8_t* extradata, int size,
                                                           int width, int height,
                                                           CMVideoFormatDescriptionRef* formatDesc) {
    // Parse H.264 extradata (AVCC format or Annex B)
    // For simplicity, try to find SPS and PPS NAL units
    std::vector<const uint8_t*> sps_list;
    std::vector<size_t> sps_sizes;
    std::vector<const uint8_t*> pps_list;
    std::vector<size_t> pps_sizes;

    // Simple NAL unit parsing for Annex B format (start codes)
    const uint8_t* p = extradata;
    const uint8_t* end = extradata + size;

    while (p < end - 4) {
        // Look for start code
        size_t start_code_len = 0;
        if (p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
            start_code_len = 4;
        } else if (p[0] == 0 && p[1] == 0 && p[2] == 1) {
            start_code_len = 3;
        }

        if (start_code_len > 0) {
            p += start_code_len;
            if (p >= end) break;

            uint8_t nal_type = p[0] & 0x1F;
            const uint8_t* nal_start = p;

            // Find next start code
            const uint8_t* next = p + 1;
            while (next < end - 3) {
                if ((next[0] == 0 && next[1] == 0 && next[2] == 0 && next[3] == 1) ||
                    (next[0] == 0 && next[1] == 0 && next[2] == 1)) {
                    break;
                }
                next++;
            }

            size_t nal_size = next - nal_start;

            if (nal_type == 7) { // SPS
                sps_list.push_back(nal_start);
                sps_sizes.push_back(nal_size);
            } else if (nal_type == 8) { // PPS
                pps_list.push_back(nal_start);
                pps_sizes.push_back(nal_size);
            }

            p = next;
        } else {
            p++;
        }
    }

    if (sps_list.empty() || pps_list.empty()) {
        // Try AVCC format (extradata[0..3] = configurationVersion, profile, etc.)
        // Just create a basic format description with dimensions
        return CMVideoFormatDescriptionCreate(nullptr, kCMVideoCodecType_H264,
                                               width, height, nullptr, formatDesc);
    }

    // Combine SPS and PPS into a single array
    std::vector<const uint8_t*> param_set_pointers;
    std::vector<size_t> param_set_sizes;

    // Add all SPS
    for (size_t i = 0; i < sps_list.size() && i < 1; i++) {
        param_set_pointers.push_back(sps_list[i]);
        param_set_sizes.push_back(sps_sizes[i]);
    }
    // Add all PPS
    for (size_t i = 0; i < pps_list.size() && i < 1; i++) {
        param_set_pointers.push_back(pps_list[i]);
        param_set_sizes.push_back(pps_sizes[i]);
    }

    return CMVideoFormatDescriptionCreateFromH264ParameterSets(nullptr,
                                                              param_set_pointers.size(),
                                                              param_set_pointers.data(),
                                                              param_set_sizes.data(),
                                                              4,  // NALUnitHeaderLength
                                                              formatDesc);
}

OSStatus VideoToolboxDecoder::CreateHEVCFormatDescription(const uint8_t* extradata, int size,
                                                           int width, int height,
                                                           CMVideoFormatDescriptionRef* formatDesc) {
    // For HEVC, create basic format description
    // Full implementation would parse VPS/SPS/PPS similar to H.264
    return CMVideoFormatDescriptionCreate(nullptr, kCMVideoCodecType_HEVC,
                                           width, height, nullptr, formatDesc);
}

OSStatus VideoToolboxDecoder::CreateDecompressionSession() {
    if (!format_desc_) return -1;

    VTDecompressionOutputCallbackRecord callback_record;
    callback_record.decompressionOutputCallback = OutputCallback;
    callback_record.decompressionOutputRefCon = this;

    // Set image buffer attributes for optimal performance
    // Request NV12 format (most efficient for VideoToolbox)
    const void* keys[] = {
        kCVPixelBufferPixelFormatTypeKey,
        kCVPixelBufferWidthKey,
        kCVPixelBufferHeightKey
    };
    int64_t nv12Format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    int64_t w = width_;
    int64_t h = height_;
    const void* values[] = {
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &nv12Format),
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &w),
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &h)
    };

    CFDictionaryRef image_buffer_attrs = CFDictionaryCreate(nullptr, keys, values, 3,
                                                            nullptr, nullptr);

    OSStatus status = VTDecompressionSessionCreate(nullptr, format_desc_, nullptr,
                                                  image_buffer_attrs, &callback_record, &session_);

    CFRelease(values[0]);
    CFRelease(values[1]);
    CFRelease(values[2]);
    CFRelease(image_buffer_attrs);

    return status;
}

AVFramePtr VideoToolboxDecoder::Decode(const AVPacketPtr& packet) {
    // VideoToolbox hardware decoding is complex and requires careful handling
    // of NAL units and sample buffer creation. For now, return nullptr to
    // trigger fallback to software decoder.
    // TODO: Full implementation with proper NAL unit parsing and sample buffer creation
    LOG_WARNING("VideoToolboxDecoder", "Hardware decoding not yet fully implemented, using software fallback");
    return nullptr;
}

void VideoToolboxDecoder::Flush() {
    if (session_) {
        VTDecompressionSessionFinishDelayedFrames(session_);
    }
}

void VideoToolboxDecoder::Close() {
    if (session_) {
        VTDecompressionSessionInvalidate(session_);
        CFRelease(session_);
        session_ = nullptr;
    }
    if (format_desc_) {
        CFRelease(format_desc_);
        format_desc_ = nullptr;
    }
    if (pixel_buffer_) {
        CFRelease(pixel_buffer_);
        pixel_buffer_ = nullptr;
    }
}

void VideoToolboxDecoder::OutputCallback(void* decompression_output_ref_con,
                                         void* source_frame_ref_con,
                                         OSStatus status,
                                         VTDecodeInfoFlags info_flags,
                                         CVImageBufferRef image_buffer,
                                         CMTime presentation_time_stamp,
                                         CMTime presentation_duration) {
    auto* decoder = static_cast<VideoToolboxDecoder*>(decompression_output_ref_con);
    std::lock_guard<std::mutex> lock(decoder->mutex_);

    decoder->decode_completed_ = true;
    decoder->decode_status_ = status;

    if (status == noErr && image_buffer) {
        decoder->pixel_buffer_ = (CVPixelBufferRef)CFRetain(image_buffer);
    }

    decoder->decode_done_.notify_one();
}

std::unique_ptr<IDecoder> CreateVideoToolboxDecoder() {
    return std::make_unique<VideoToolboxDecoder>();
}

bool IsHardwareDecoderAvailable(AVCodecID codec_id) {
    if (codec_id == AV_CODEC_ID_H264 || codec_id == AV_CODEC_ID_HEVC) {
        return true;
    }
    return false;
}

} // namespace avsdk
