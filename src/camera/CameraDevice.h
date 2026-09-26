#pragma once

#include <string>
#include <vector>

#include "camera/CameraFormat.h"

namespace haocam {

struct CameraDevice {
    std::string id;          // stable symbolc link / device path
    std::string displayName; // friendly name
    std::vector<CameraFormatDesc> formats; // native modes (filled on demand)
};

} // namespace haocam
