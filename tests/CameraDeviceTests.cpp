// Unit tests for camera auto-selection (physical before virtual).

#include "camera/CameraDevice.h"

#include "test_main.h"

namespace {

haocam::CameraDevice makeDevice(const std::string& id, const std::string& name,
                                bool virtualDevice) {
    haocam::CameraDevice device;
    device.id = id;
    device.displayName = name;
    device.isVirtual = virtualDevice;
    return device;
}

} // namespace

HAOCAM_TEST(camera_prefers_physical_over_virtual) {
    // Empty list -> nullptr.
    HAOCAM_EXPECT(haocam::preferredCameraDevice({}) == nullptr);

    // Physical cameras win regardless of order.
    std::vector<haocam::CameraDevice> mixed = {
        makeDevice(R"(\\?\root#media#0001#global)", "DroidCam Video", true),
        makeDevice(R"(\\?\usb#vid_1234&pid_5678#global)", "WebCamera", false),
        makeDevice(R"(\\?\root#media#0002#global)", "SplitCam", true),
    };
    const haocam::CameraDevice* picked = haocam::preferredCameraDevice(mixed);
    HAOCAM_EXPECT(picked != nullptr);
    HAOCAM_EXPECT_EQ(picked->displayName, "WebCamera");
    HAOCAM_EXPECT_EQ(picked->isVirtual, false);

    // Only virtual cameras -> fall back to the first one.
    std::vector<haocam::CameraDevice> virtualOnly = {
        makeDevice(R"(\\?\root#media#0001#global)", "Snap Camera", true),
        makeDevice(R"(\\?\root#media#0002#global)", "DroidCam Video", true),
    };
    picked = haocam::preferredCameraDevice(virtualOnly);
    HAOCAM_EXPECT_EQ(picked->displayName, "Snap Camera");

    // Physical cameras keep enumeration order.
    std::vector<haocam::CameraDevice> physicalOnly = {
        makeDevice(R"(\\?\usb#vid_0001&pid_0002#global)", "Cam A", false),
        makeDevice(R"(\\?\usb#vid_0003&pid_0004#global)", "Cam B", false),
    };
    picked = haocam::preferredCameraDevice(physicalOnly);
    HAOCAM_EXPECT_EQ(picked->displayName, "Cam A");
}
