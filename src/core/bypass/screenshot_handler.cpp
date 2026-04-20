#include "screenshot_handler.h"
#include "avsdk/logger.h"
extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include <algorithm>
#include <cstring>

namespace avsdk {

ScreenshotHandler::ScreenshotHandler()
    : width_(0), height_(0), hasData_(false) {
}

ScreenshotHandler::~ScreenshotHandler() {
    Clear();
}

int ScreenshotHandler::Capture(const VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    int result = ConvertToRGB(frame, rgbData_, width_, height_);
    if (result == 0) {
        hasData_ = true;
    }
    return result;
}

int ScreenshotHandler::Save(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!hasData_ || rgbData_.empty()) {
        LOG_ERROR("Screenshot", "No data to save");
        return -1;
    }

    // Detect format from file extension
    std::string ext = path;
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = path.substr(dotPos);
    }

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    AVCodecID codec_id;
    const char* format_name;
    if (ext == ".png") {
        codec_id = AV_CODEC_ID_PNG;
        format_name = "png";
    } else if (ext == ".jpg" || ext == ".jpeg") {
        codec_id = AV_CODEC_ID_MJPEG;
        format_name = "jpg";
    } else if (ext == ".bmp") {
        codec_id = AV_CODEC_ID_BMP;
        format_name = "bmp";
    } else {
        // Default to PNG
        codec_id = AV_CODEC_ID_PNG;
        format_name = "png";
    }

    // Find encoder
    const AVCodec* codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        LOG_ERROR("Screenshot", "Encoder not found for format: " + std::string(format_name));
        return -1;
    }

    // Allocate codec context
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        LOG_ERROR("Screenshot", "Failed to allocate codec context");
        return -1;
    }

    // Set codec parameters
    codec_ctx->width = width_;
    codec_ctx->height = height_;
    codec_ctx->time_base = {1, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_RGB24;

    if (codec_id == AV_CODEC_ID_MJPEG) {
        codec_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    }

    // Open codec
    int ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        LOG_ERROR("Screenshot", "Failed to open codec");
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    // Allocate frame
    AVFrame* av_frame = av_frame_alloc();
    if (!av_frame) {
        LOG_ERROR("Screenshot", "Failed to allocate frame");
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    av_frame->width = width_;
    av_frame->height = height_;
    av_frame->format = codec_ctx->pix_fmt;

    if (codec_id == AV_CODEC_ID_MJPEG) {
        // For MJPEG, need to convert RGB to YUV420P
        struct SwsContext* sws_ctx = sws_getContext(
            width_, height_, AV_PIX_FMT_RGB24,
            width_, height_, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!sws_ctx) {
            LOG_ERROR("Screenshot", "Failed to create sws context");
            av_frame_free(&av_frame);
            avcodec_free_context(&codec_ctx);
            return -1;
        }

        // Allocate YUV frame
        ret = av_frame_get_buffer(av_frame, 0);
        if (ret < 0) {
            LOG_ERROR("Screenshot", "Failed to allocate frame buffer");
            sws_freeContext(sws_ctx);
            av_frame_free(&av_frame);
            avcodec_free_context(&codec_ctx);
            return -1;
        }

        // Convert RGB to YUV
        const uint8_t* src_data[1] = { rgbData_.data() };
        int src_linesize[1] = { width_ * 3 };
        sws_scale(sws_ctx, src_data, src_linesize, 0, height_,
                  av_frame->data, av_frame->linesize);

        sws_freeContext(sws_ctx);
    } else {
        // For PNG and BMP, RGB24 is fine
        av_frame->data[0] = rgbData_.data();
        av_frame->linesize[0] = width_ * 3;
    }

    // Encode frame
    ret = avcodec_send_frame(codec_ctx, av_frame);
    if (ret < 0) {
        LOG_ERROR("Screenshot", "Failed to send frame to encoder");
        if (codec_id == AV_CODEC_ID_MJPEG) {
            av_frame_free(&av_frame);
        }
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    // Receive encoded packet
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("Screenshot", "Failed to allocate packet");
        if (codec_id == AV_CODEC_ID_MJPEG) {
            av_frame_free(&av_frame);
        }
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    ret = avcodec_receive_packet(codec_ctx, packet);
    if (ret < 0) {
        LOG_ERROR("Screenshot", "Failed to receive packet from encoder");
        av_packet_free(&packet);
        if (codec_id == AV_CODEC_ID_MJPEG) {
            av_frame_free(&av_frame);
        }
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    // Write to file
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) {
        LOG_ERROR("Screenshot", "Failed to open file: " + path);
        av_packet_free(&packet);
        if (codec_id == AV_CODEC_ID_MJPEG) {
            av_frame_free(&av_frame);
        }
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    fwrite(packet->data, 1, packet->size, fp);
    fclose(fp);

    // Cleanup
    av_packet_free(&packet);
    if (codec_id == AV_CODEC_ID_MJPEG) {
        av_frame_free(&av_frame);
    }
    avcodec_free_context(&codec_ctx);

    LOG_INFO("Screenshot", "Saved screenshot to: " + path);
    return 0;
}

int ScreenshotHandler::GetData(std::vector<uint8_t>& data, int& width, int& height) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!hasData_ || rgbData_.empty()) {
        return -1;
    }

    data = rgbData_;
    width = width_;
    height = height_;
    return 0;
}

int ScreenshotHandler::GetPNG(std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!hasData_ || rgbData_.empty()) {
        LOG_ERROR("Screenshot", "No data to encode");
        return -1;
    }

    // Find PNG encoder
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!codec) {
        LOG_ERROR("Screenshot", "PNG encoder not found");
        return -1;
    }

    // Allocate codec context
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        LOG_ERROR("Screenshot", "Failed to allocate codec context");
        return -1;
    }

    // Set codec parameters
    codec_ctx->width = width_;
    codec_ctx->height = height_;
    codec_ctx->time_base = {1, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_RGB24;

    // Open codec
    int ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        LOG_ERROR("Screenshot", "Failed to open codec");
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    // Allocate frame
    AVFrame* av_frame = av_frame_alloc();
    if (!av_frame) {
        LOG_ERROR("Screenshot", "Failed to allocate frame");
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    av_frame->width = width_;
    av_frame->height = height_;
    av_frame->format = AV_PIX_FMT_RGB24;
    av_frame->data[0] = rgbData_.data();
    av_frame->linesize[0] = width_ * 3;

    // Encode frame
    ret = avcodec_send_frame(codec_ctx, av_frame);
    if (ret < 0) {
        LOG_ERROR("Screenshot", "Failed to send frame to encoder");
        av_frame_free(&av_frame);
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    // Receive encoded packet
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("Screenshot", "Failed to allocate packet");
        av_frame_free(&av_frame);
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    ret = avcodec_receive_packet(codec_ctx, packet);
    if (ret < 0) {
        LOG_ERROR("Screenshot", "Failed to receive packet from encoder");
        av_packet_free(&packet);
        av_frame_free(&av_frame);
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    // Copy data to output
    data.resize(packet->size);
    memcpy(data.data(), packet->data, packet->size);

    // Cleanup
    av_packet_free(&packet);
    av_frame_free(&av_frame);
    avcodec_free_context(&codec_ctx);

    return 0;
}

bool ScreenshotHandler::HasData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hasData_ && !rgbData_.empty();
}

void ScreenshotHandler::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    rgbData_.clear();
    width_ = 0;
    height_ = 0;
    hasData_ = false;
}

// IDataBypass interface methods
void ScreenshotHandler::OnRawPacket(const EncodedPacket& /*packet*/) {
    // Not used for screenshot
}

void ScreenshotHandler::OnDecodedVideoFrame(const VideoFrame& frame) {
    // Capture video frame for screenshot
    Capture(frame);
}

void ScreenshotHandler::OnDecodedAudioFrame(const AudioFrame& /*frame*/) {
    // Not used for screenshot
}

void ScreenshotHandler::OnPreRenderVideoFrame(VideoFrame& /*frame*/) {
    // Could capture here too, but we capture on decode
}

void ScreenshotHandler::OnPreRenderAudioFrame(AudioFrame& /*frame*/) {
    // Not used for screenshot
}

void ScreenshotHandler::OnEncodedPacket(const EncodedPacket& /*packet*/) {
    // Not used for screenshot
}

// Static Create method
std::shared_ptr<ScreenshotHandler> ScreenshotHandler::Create() {
    return std::make_shared<ScreenshotHandler>();
}

// Factory function
std::shared_ptr<IScreenshotHandler> CreateScreenshotHandler() {
    return ScreenshotHandler::Create();
}

// Convert VideoFrame to RGB24 using sws_scale
int ScreenshotHandler::ConvertToRGB(const VideoFrame& frame, std::vector<uint8_t>& output,
                                    int& outWidth, int& outHeight) {
    if (!frame.data[0]) {
        LOG_ERROR("Screenshot", "Invalid frame data");
        return -1;
    }

    int srcWidth = frame.resolution.width;
    int srcHeight = frame.resolution.height;
    AVPixelFormat srcFormat = static_cast<AVPixelFormat>(frame.format);

    // If format is RGB24 already, just copy
    if (srcFormat == AV_PIX_FMT_RGB24) {
        outWidth = srcWidth;
        outHeight = srcHeight;
        output.resize(srcWidth * srcHeight * 3);
        memcpy(output.data(), frame.data[0], srcWidth * srcHeight * 3);
        return 0;
    }

    // Create sws context for format conversion
    struct SwsContext* sws_ctx = sws_getContext(
        srcWidth, srcHeight, srcFormat,
        srcWidth, srcHeight, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!sws_ctx) {
        LOG_ERROR("Screenshot", "Failed to create sws context");
        return -1;
    }

    // Allocate output buffer (RGB24)
    outWidth = srcWidth;
    outHeight = srcHeight;
    output.resize(srcWidth * srcHeight * 3);

    // Destination data
    uint8_t* dstData[1] = { output.data() };
    int dstLinesize[1] = { srcWidth * 3 };

    // Perform conversion
    sws_scale(sws_ctx, frame.data, frame.linesize, 0, srcHeight, dstData, dstLinesize);

    // Free sws context
    sws_freeContext(sws_ctx);

    return 0;
}

} // namespace avsdk
