/**
 * HuanFace Result/Error Implementation — Phase 3
 */

#include "result.h"

namespace huanface {

const char* ResultToString(HFResult result) {
    switch (result) {
        case HF_RESULT_OK: return "OK";
        case HF_RESULT_FAIL: return "FAIL";
        case HF_RESULT_NOT_INITIALIZED: return "NOT_INITIALIZED";
        case HF_RESULT_INVALID_PARAM: return "INVALID_PARAM";
        case HF_RESULT_NOT_SUPPORTED: return "NOT_SUPPORTED";
        case HF_RESULT_OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case HF_RESULT_FILE_NOT_FOUND: return "FILE_NOT_FOUND";
        case HF_RESULT_BUNDLE_INVALID: return "BUNDLE_INVALID";
        case HF_RESULT_FACE_NOT_DETECTED: return "FACE_NOT_DETECTED";
        default: return "UNKNOWN";
    }
}

} // namespace huanface

// C ABI implementations
extern "C" {

const char* HF_GetVersion() {
    return "0.3.0-phase3";
}

const char* HF_GetResultString(HFResult result) {
    return huanface::ResultToString(result);
}

} // extern "C"
