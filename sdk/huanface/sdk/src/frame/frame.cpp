/**
 * HuanFace Frame Implementation — Phase 3
 */

#include "frame.h"
#include <sstream>

namespace huanface {

bool FrameValidator::IsValidFormat(HFFormat format) {
    switch (format) {
        case HF_FORMAT_RGBA8:
        case HF_FORMAT_BGRA8:
        case HF_FORMAT_RGB8:
        case HF_FORMAT_BGR8:
        case HF_FORMAT_NV12:
        case HF_FORMAT_YUV420P:
        case HF_FORMAT_R8:
        case HF_FORMAT_R32F:
            return true;
        default:
            return false;
    }
}

bool FrameValidator::IsYUVFormat(HFFormat format) {
    return format == HF_FORMAT_NV12 || format == HF_FORMAT_YUV420P;
}

int FrameValidator::GetBytesPerPixel(HFFormat format) {
    switch (format) {
        case HF_FORMAT_RGBA8: return 4;
        case HF_FORMAT_BGRA8: return 4;
        case HF_FORMAT_RGB8: return 3;
        case HF_FORMAT_BGR8: return 3;
        case HF_FORMAT_R8: return 1;
        case HF_FORMAT_R32F: return 4;
        case HF_FORMAT_NV12: return 1; // Y plane, UV interleaved handled separately
        case HF_FORMAT_YUV420P: return 1;
        default: return 0;
    }
}

std::string FrameValidator::FormatToString(HFFormat format) {
    switch (format) {
        case HF_FORMAT_RGBA8: return "RGBA8";
        case HF_FORMAT_BGRA8: return "BGRA8";
        case HF_FORMAT_RGB8: return "RGB8";
        case HF_FORMAT_BGR8: return "BGR8";
        case HF_FORMAT_NV12: return "NV12";
        case HF_FORMAT_YUV420P: return "YUV420P";
        case HF_FORMAT_R8: return "R8";
        case HF_FORMAT_R32F: return "R32F";
        case HF_FORMAT_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

HFResult FrameValidator::Validate(const HFFrameC* frame, std::string* outError) {
    auto setError = [&](const std::string& msg) {
        if (outError) *outError = msg;
    };

    if (!frame) {
        setError("Frame is null");
        return HF_RESULT_INVALID_PARAM;
    }

    if (frame->width <= 0 || frame->height <= 0) {
        setError("Invalid dimensions: width=" + std::to_string(frame->width) + " height=" + std::to_string(frame->height));
        return HF_RESULT_INVALID_PARAM;
    }

    if (!IsValidFormat(frame->format)) {
        setError("Invalid format: " + std::to_string((int)frame->format));
        return HF_RESULT_INVALID_PARAM;
    }

    if (frame->rotation != 0 && frame->rotation != 90 && frame->rotation != 180 && frame->rotation != 270) {
        setError("Invalid rotation: " + std::to_string(frame->rotation) + " must be 0,90,180,270");
        return HF_RESULT_INVALID_PARAM;
    }

    bool hasCpuData = (frame->data != nullptr);
    bool hasGpuTexture = (frame->gpuTexture != nullptr);

    if (!hasCpuData && !hasGpuTexture) {
        setError("Frame has neither CPU data nor GPU texture");
        return HF_RESULT_INVALID_PARAM;
    }

    if (hasCpuData) {
        // Validate stride
        int bpp = GetBytesPerPixel(frame->format);
        if (IsYUVFormat(frame->format)) {
            // For NV12, stride should be >= width
            if (frame->stride < frame->width) {
                setError("Invalid stride for YUV format: stride=" + std::to_string(frame->stride) + " width=" + std::to_string(frame->width));
                return HF_RESULT_INVALID_PARAM;
            }
            // For NV12, dataU should be present
            if (frame->format == HF_FORMAT_NV12) {
                if (!frame->dataU) {
                    setError("NV12 format requires dataU (UV plane)");
                    return HF_RESULT_INVALID_PARAM;
                }
                if (frame->strideU < frame->width) {
                    setError("Invalid strideU for NV12: " + std::to_string(frame->strideU));
                    return HF_RESULT_INVALID_PARAM;
                }
            }
            if (frame->format == HF_FORMAT_YUV420P) {
                if (!frame->dataU || !frame->dataV) {
                    setError("YUV420P requires dataU and dataV");
                    return HF_RESULT_INVALID_PARAM;
                }
            }
        } else {
            // RGB/RGBA/R8/R32F
            int minStride = frame->width * bpp;
            if (frame->stride < minStride) {
                setError("Invalid stride: stride=" + std::to_string(frame->stride) + " minRequired=" + std::to_string(minStride) + " for format " + FormatToString(frame->format));
                return HF_RESULT_INVALID_PARAM;
            }
        }
    }

    // GPU texture validation: if has GPU texture, format must be compatible
    // For Phase 3, we just check format is valid (already done)
    // Additional checks for nativeHandle etc. not needed for minimal

    return HF_RESULT_OK;
}

} // namespace huanface
