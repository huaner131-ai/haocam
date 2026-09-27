#pragma once

#include <string>
#include <vector>

#include "camera/CameraFormat.h"

namespace haocam {

struct CameraDevice {
    std::string id;          // stable symbolc link / device path
    std::string displayName; // friendly name
    std::vector<CameraFormatDesc> formats; // native modes (filled on demand)
    bool isVirtual = false;  // software enumerator (root#/swd#): virtual cams
};

// Auto-selection: prefer the first PHYSICAL camera. Virtual drivers
// (DroidCam / SplitCam / Snap virtual cam - symlink under root#media) sit
// idle without an active source and never deliver frames, which previously
// made the app pick them by accident and starve the preview.
// Returns nullptr only for an empty list.
inline const CameraDevice* preferredCameraDevice(
    const std::vector<CameraDevice>& devices) {
    for (const auto& device : devices) {
        if (!device.isVirtual) return &device;
    }
    return devices.empty() ? nullptr : &devices.front();
}

} // namespace haocam
