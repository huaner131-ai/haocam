/**
 * HuanFace Result/Error System — Phase 3
 * Thread-safe error handling, deterministic codes
 */

#pragma once
#include "../../include/huanface_c_api.h"
#include <string>

namespace huanface {

const char* ResultToString(HFResult result);
HFResult ResultFromString(const std::string& str); // for debugging

} // namespace huanface
