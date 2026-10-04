/**
 * HuanFace Frame System — Phase 3
 * HFFrame abstraction with CPU/GPU separation
 */

#pragma once
#include "../../include/huanface_c_api.h"
#include <string>

namespace huanface {

class FrameValidator {
public:
    static HFResult Validate(const HFFrameC* frame, std::string* outError = nullptr);
    static int GetBytesPerPixel(HFFormat format);
    static bool IsValidFormat(HFFormat format);
    static bool IsYUVFormat(HFFormat format);
    static std::string FormatToString(HFFormat format);
};

} // namespace huanface
