#pragma once
#include "avsdk/decoder.h"

namespace avsdk {

enum class HWDecoderType {
    kNone,
    kVideoToolbox,  // macOS/iOS
    kMediaCodec,    // Android
    kDXVA,          // Windows
};

// Factory for hardware decoders
std::unique_ptr<IDecoder> CreateHardwareDecoder(HWDecoderType type);
std::unique_ptr<IDecoder> CreateVideoToolboxDecoder();

// Check if hardware decoder is available for codec
bool IsHardwareDecoderAvailable(CodecType codec_type);

} // namespace avsdk
