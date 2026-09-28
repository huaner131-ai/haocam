#include "camera/MediaFoundationCapture.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>

#include "camera/CameraFormat.h"
#include "core/logging/Logger.h"
#include "core/threading/NamedThread.h"
#include "frame/FrameBuffer.h"
#include "graphics/D3D11/D3D11Texture.h"
#include "graphics/D3D11/D3D11MultithreadCompat.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <combaseapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

using Microsoft::WRL::ComPtr;

namespace haocam {

namespace {
bool sameMode(const CameraFormatDesc& a, const CameraFormatDesc& b) {
    return a.resolution == b.resolution && a.fpsNumerator == b.fpsNumerator &&
           a.fpsDenominator == b.fpsDenominator && a.pixelFormat == b.pixelFormat;
}
} // namespace


namespace {
constexpr const char* kCategory = "camera";

std::string wideToUtf8(const wchar_t* text) {
    if (!text) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size > 0 ? size - 1 : 0), '\0');
    if (size > 1) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
    }
    return result;
}

GUID fourCcToMediaSubtype(const std::string& fourcc) {
    if (fourcc == "NV12") return MFVideoFormat_NV12;
    if (fourcc == "MJPG") return MFVideoFormat_MJPG;
    if (fourcc == "H264") return MFVideoFormat_H264;
    if (fourcc == "YUY2") return MFVideoFormat_YUY2;
    return MFVideoFormat_NV12;
}

// RAII for COM + Media Foundation on the camera thread. Objects created on
// this thread are released here as well.
struct MfThreadContext {
    bool comOk = false;
    bool mfOk = false;

    MfThreadContext() {
        comOk = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
        mfOk = comOk && SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE));
    }
    ~MfThreadContext() {
        if (mfOk) MFShutdown();
        if (comOk) CoUninitialize();
    }
};

ComPtr<ID3D11Device> createMfDevice(void* externalDevice) {
    if (externalDevice) {
        ComPtr<ID3D11Device> device(static_cast<ID3D11Device*>(externalDevice));
        return device; // AddRef via ComPtr
    }
    const UINT flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    };
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL obtained{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels,
                                   ARRAYSIZE(levels), D3D11_SDK_VERSION, device.GetAddressOf(),
                                   &obtained, context.GetAddressOf());
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels,
                               ARRAYSIZE(levels), D3D11_SDK_VERSION, device.GetAddressOf(),
                               &obtained, context.GetAddressOf());
        if (SUCCEEDED(hr)) {
            HAOCAM_LOG_WARN(kCategory, "Media Foundation using WARP software device");
        }
    }
    return device;
}

bool enableMultithreadProtect(ID3D11Device* device) {
    return gfx::enableDeviceMultithreadProtect(device);
}

bool createDxgiManager(ID3D11Device* device, ComPtr<IMFDXGIDeviceManager>& out) {
    UINT resetToken = 0;
    if (FAILED(MFCreateDXGIDeviceManager(&resetToken, &out))) return false;
    return SUCCEEDED(out->ResetDevice(device, resetToken));
}

std::vector<CameraFormatDesc> enumerateNativeFormats(IMFSourceReader* reader, DWORD stream) {
    std::vector<CameraFormatDesc> formats;
    for (DWORD i = 0;; ++i) {
        ComPtr<IMFMediaType> type;
        if (FAILED(reader->GetNativeMediaType(stream, i, &type))) break;
        UINT32 width = 0, height = 0;
        if (FAILED(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height))) continue;
        UINT32 num = 30, den = 1;
        if (FAILED(MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &num, &den)) || den == 0) {
            num = 30;
            den = 1;
        }
        GUID subtype{};
        char fourcc[5] = {0};
        if (SUCCEEDED(type->GetGUID(MF_MT_SUBTYPE, &subtype))) {
            std::memcpy(fourcc, &subtype.Data1, 4);
            for (int c = 0; c < 4; ++c) {
                if (static_cast<unsigned char>(fourcc[c]) < 0x20) fourcc[c] = '?';
            }
        }
        formats.push_back({{width, height}, num, den, fourcc});
    }
    return dedupeFormats(formats);
}

ComPtr<IMFActivate> findActivation(const std::string& symbolicLink) {
    ComPtr<IMFAttributes> attributes;
    if (FAILED(MFCreateAttributes(attributes.GetAddressOf(), 1))) return nullptr;
    if (FAILED(attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                                   MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) {
        return nullptr;
    }
    IMFActivate** activates = nullptr;
    UINT32 count = 0;
    if (FAILED(MFEnumDeviceSources(attributes.Get(), &activates, &count))) return nullptr;

    ComPtr<IMFActivate> found;
    for (UINT32 i = 0; i < count; ++i) {
        LPWSTR symlink = nullptr;
        UINT32 length = 0;
        if (SUCCEEDED(activates[i]->GetAllocatedString(
                MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &symlink, &length))) {
            if (wideToUtf8(symlink) == symbolicLink) found = activates[i];
            CoTaskMemFree(symlink);
        }
        if (!found) activates[i]->Release();
        if (found) break;
    }
    CoTaskMemFree(activates);
    return found;
}

} // namespace

void MediaFoundationCapture::rememberFailedMode(const CameraFormatDesc& mode) {
    std::lock_guard<std::mutex> lock(m_failedModesMutex);
    for (const auto& known : m_failedModes) {
        if (sameMode(known, mode)) return;
    }
    m_failedModes.push_back(mode);
    HAOCAM_LOG_WARN(kCategory,
                    "Mode blacklisted after streaming failure: {} ({} mode(s) excluded "
                    "from now on)",
                    describeFormat(mode), m_failedModes.size());
}

std::vector<CameraFormatDesc> MediaFoundationCapture::usableFormats(
    const std::vector<CameraFormatDesc>& native) const {
    std::lock_guard<std::mutex> lock(m_failedModesMutex);
    if (m_failedModes.empty()) return native;
    std::vector<CameraFormatDesc> usable;
    for (const auto& f : native) {
        bool failed = false;
        for (const auto& bad : m_failedModes) {
            if (sameMode(bad, f)) { failed = true; break; }
        }
        if (!failed) usable.push_back(f);
    }
    return usable;
}


MediaFoundationCapture::~MediaFoundationCapture() { stop(); }

std::vector<CameraDevice> MediaFoundationCapture::enumerateDevices() {
    MfThreadContext mf;

    std::vector<CameraDevice> devices;
    ComPtr<IMFAttributes> attributes;
    if (FAILED(MFCreateAttributes(attributes.GetAddressOf(), 1))) return devices;
    if (FAILED(attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                                   MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) {
        return devices;
    }

    IMFActivate** activates = nullptr;
    UINT32 count = 0;
    const HRESULT hr = MFEnumDeviceSources(attributes.Get(), &activates, &count);
    if (FAILED(hr)) {
        HAOCAM_LOG_WARN(kCategory, "MFEnumDeviceSources failed (hr=0x{:08X})",
                        static_cast<unsigned>(hr));
        return devices;
    }

    for (UINT32 i = 0; i < count; ++i) {
        CameraDevice device;
        LPWSTR name = nullptr;
        UINT32 length = 0;
        if (SUCCEEDED(activates[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                                                       &name, &length))) {
            device.displayName = wideToUtf8(name);
            CoTaskMemFree(name);
        }
        LPWSTR symlink = nullptr;
        if (SUCCEEDED(activates[i]->GetAllocatedString(
                MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &symlink, &length))) {
            device.id = wideToUtf8(symlink);
            // Physical webcams enumerate under usb#; software cameras
            // (DroidCam, SplitCam, Snap virtual cam, ...) under root#/swd#.
            std::string lower = device.id;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            device.isVirtual = lower.find("usb#") == std::string::npos;
            CoTaskMemFree(symlink);
        }
        // Native modes are probed when the device opens (opening every camera
        // to list modes is slow and can wake privacy LEDs).
        activates[i]->Release();
        if (!device.id.empty()) {
            HAOCAM_LOG_INFO(kCategory, "Device {}: '{}' ({})", devices.size() + 1,
                            device.displayName.empty() ? device.id : device.displayName,
                            device.isVirtual ? "virtual" : "physical");
            devices.push_back(std::move(device));
        }
    }
    CoTaskMemFree(activates);
    return devices;
}

bool MediaFoundationCapture::start(const std::string& deviceId,
                                   const CameraFormatPreference& preferenceIn) {
    stop();
    if (deviceId.empty()) {
        HAOCAM_LOG_ERROR(kCategory,
                         "start() refused: deviceId is EMPTY (caller bug upstream)");
        if (m_callbacks.onStateChanged) {
            m_callbacks.onStateChanged(CameraState::Failed, "No device id");
        }
        return false;
    }
    HAOCAM_LOG_INFO(kCategory, "Opening device (id starts with '{}')",
                    deviceId.substr(0, deviceId.find('#') + 1));

    CameraFormatPreference preference = preferenceIn;
    if (m_hasRequestedFormat.load()) {
        preference.resolution = m_requestedFormat.resolution;
        preference.fps = m_requestedFormat.fps();
    }

    m_state = CameraState::Starting;
    m_stopRequested = false;
    m_thread = std::thread([this, deviceId, preference] { run(deviceId, preference); });
    return true;
}

void MediaFoundationCapture::stop() {
    m_stopRequested = true;
    if (m_thread.joinable()) m_thread.join();
    m_state = CameraState::Idle;
}

bool MediaFoundationCapture::setFormat(const CameraFormatDesc& format) {
    m_requestedFormat = format;
    m_hasRequestedFormat = true;
    // Applied on the next start(); CameraManager restarts the source to
    // apply a user-selected mode (restarting capture is the reliable path).
    return true;
}

bool MediaFoundationCapture::setExposure(double) { return false; }
bool MediaFoundationCapture::setWhiteBalance(double) { return false; }
bool MediaFoundationCapture::setFocus(double) { return false; }
bool MediaFoundationCapture::setZoom(double) { return false; }

void MediaFoundationCapture::run(const std::string& deviceId,
                                 CameraFormatPreference preference) {
    core::setThreadName("haocam-capture");
    MfThreadContext mf;
    if (!mf.comOk || !mf.mfOk) {
        m_state = CameraState::Failed;
        if (m_callbacks.onStateChanged) {
            m_callbacks.onStateChanged(CameraState::Failed,
                                       "COM / Media Foundation initialization failed");
        }
        return;
    }

    // ---- GPU capture setup ----
    // DISABLED (2026-09-28, stacks captured on the user machine): sharing the
    // Qt render device with Media Foundation deadlocked the NVIDIA driver
    // stack - QSGRenderThread blocked inside d3d11.dll while haocam-capture
    // sat in mfplat.dll on the SAME device; the GUI then froze at the next
    // sync ("Not Responding", 0% CPU everywhere). Until cross-device sharing
    // (keyed-mutex shared handles) is properly implemented, MF must not touch
    // ANY D3D11 device: it delivers CPU NV12 samples and the engine thread
    // uploads each frame to the render device exactly once.
    constexpr bool kEnableGpuCapture = false;
    ComPtr<ID3D11Device> device; // null: no D3D11 device for Media Foundation
    if (kEnableGpuCapture) device = createMfDevice(m_externalDevice);
    const bool gpuCapture = device && enableMultithreadProtect(device.Get());
    ComPtr<IMFDXGIDeviceManager> dxgiManager;
    if (gpuCapture && !createDxgiManager(device.Get(), dxgiManager)) {
        HAOCAM_LOG_WARN(kCategory, "IMFDXGIDeviceManager unavailable; using CPU buffers");
    }
    if (!gpuCapture) {
        HAOCAM_LOG_INFO(kCategory,
                        "Capture: CPU NV12 samples (MF kept off all D3D11 devices; "
                        "the engine uploads each frame to the GPU once)");
    }

    // ---- Source reader ----
    ComPtr<IMFAttributes> readerAttributes;
    if (FAILED(MFCreateAttributes(readerAttributes.GetAddressOf(), 4))) {
        m_state = CameraState::Failed;
        if (m_callbacks.onStateChanged) {
            m_callbacks.onStateChanged(CameraState::Failed, "MFCreateAttributes failed");
        }
        return;
    }
    if (dxgiManager) {
        readerAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, dxgiManager.Get());
    }
    readerAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
    readerAttributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, FALSE);

    ComPtr<IMFActivate> activation = findActivation(deviceId);
    if (!activation) {
        m_state = CameraState::Reconnecting;
        if (m_callbacks.onStateChanged) {
            m_callbacks.onStateChanged(CameraState::Reconnecting,
                                       "Camera device not present (unplugged?)");
        }
        return;
    }

    ComPtr<IMFMediaSource> mediaSource;
    if (FAILED(activation->ActivateObject(IID_PPV_ARGS(mediaSource.GetAddressOf())))) {
        m_state = CameraState::Reconnecting;
        if (m_callbacks.onStateChanged) {
            m_callbacks.onStateChanged(CameraState::Reconnecting,
                                       "Camera device could not be activated");
        }
        return;
    }

    ComPtr<IMFSourceReader> reader;
    if (FAILED(MFCreateSourceReaderFromMediaSource(mediaSource.Get(),
                                                   readerAttributes.Get(),
                                                   reader.GetAddressOf()))) {
        m_state = CameraState::Failed;
        if (m_callbacks.onStateChanged) {
            m_callbacks.onStateChanged(CameraState::Failed, "MFCreateSourceReader failed");
        }
        return;
    }

    const DWORD stream = MF_SOURCE_READER_FIRST_VIDEO_STREAM;

    {
        std::lock_guard<std::mutex> lock(m_failedModesMutex);
        if (deviceId != m_lastStartedDevice) {
            m_failedModes.clear(); // new device: previous failures are irrelevant
            m_lastStartedDevice = deviceId;
        }
    }
    const std::vector<CameraFormatDesc> native = enumerateNativeFormats(reader.Get(), stream);
    if (native.empty()) {
        m_state = CameraState::Failed;
        if (m_callbacks.onStateChanged) {
            m_callbacks.onStateChanged(CameraState::Failed, "Camera exposes no video modes");
        }
        return;
    }
    // Skip modes that already failed to stream on this device; if everything
    // is blacklisted, try the full list again rather than nothing.
    std::vector<CameraFormatDesc> candidates = usableFormats(native);
    if (candidates.size() != native.size()) {
        HAOCAM_LOG_INFO(kCategory,
                        "Excluding {} previously failing mode(s) from selection",
                        native.size() - candidates.size());
    }
    if (candidates.empty()) candidates = native;
    const CameraFormatDesc* best = pickBestFormat(candidates, preference);
    if (!best) best = &native.front();
    HAOCAM_LOG_INFO(kCategory, "Selected camera mode: {}", describeFormat(*best));

    // Request NV12 at the best mode (MF inserts converters when needed).
    ComPtr<IMFMediaType> outputType;
    MFCreateMediaType(outputType.GetAddressOf());
    outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, best->resolution.width,
                       best->resolution.height);
    MFSetAttributeRatio(outputType.Get(), MF_MT_FRAME_RATE, best->fpsNumerator,
                        best->fpsDenominator);
    if (FAILED(reader->SetCurrentMediaType(stream, nullptr, outputType.Get()))) {
        HAOCAM_LOG_WARN(kCategory,
                        "NV12 output rejected; falling back to native mode (CPU frames)");
        reader->SetCurrentMediaType(stream, nullptr, nullptr);
    }

    ComPtr<IMFMediaType> actual;
    UINT32 w = best->resolution.width, h = best->resolution.height;
    UINT32 fn = best->fpsNumerator, fd = best->fpsDenominator;
    if (SUCCEEDED(reader->GetCurrentMediaType(stream, &actual))) {
        MFGetAttributeSize(actual.Get(), MF_MT_FRAME_SIZE, &w, &h);
        MFGetAttributeRatio(actual.Get(), MF_MT_FRAME_RATE, &fn, &fd);
        GUID subtype{};
        char fourcc[5] = {0};
        if (SUCCEEDED(actual->GetGUID(MF_MT_SUBTYPE, &subtype))) {
            std::memcpy(fourcc, &subtype.Data1, 4);
            for (int c = 0; c < 4; ++c) {
                if (static_cast<unsigned char>(fourcc[c]) < 0x20) fourcc[c] = '?';
            }
            m_activeFormat.pixelFormat = fourcc;
        }
    }
    m_activeFormat.resolution = {w, h};
    m_activeFormat.fpsNumerator = fn ? fn : 30;
    m_activeFormat.fpsDenominator = fd ? fd : 1;
    HAOCAM_LOG_INFO(kCategory, "Camera opened: {}", describeFormat(m_activeFormat));

    m_state = CameraState::Running;
    if (m_callbacks.onStateChanged) m_callbacks.onStateChanged(CameraState::Running, {});

    // ---- Frame pool for GPU-resident copies ----
    std::shared_ptr<gfx::D3D11TexturePool> texturePool;
    if (gpuCapture && m_activeFormat.pixelFormat == "NV12") {
        texturePool = gfx::D3D11TexturePool::create(device.Get());
    }
    gfx::D3D11TextureFactory viewFactory(device.Get());

    // ---------------- Capture loop ----------------
    uint64_t framesPublished = 0;
    while (!m_stopRequested.load()) {
        DWORD flags = 0;
        DWORD actualStream = 0;
        LONGLONG sampleTime100ns = 0;
        ComPtr<IMFSample> sample;
        const HRESULT hr =
            reader->ReadSample(stream, 0, &flags, &actualStream, &sampleTime100ns,
                               sample.GetAddressOf());
        if (FAILED(hr)) {
            char detail[64];
            std::snprintf(detail, sizeof(detail), "ReadSample failed (hr=0x%08lX)",
                          static_cast<unsigned long>(hr));
            rememberFailedMode(m_activeFormat); // reconnect must pick another mode
            m_state = CameraState::Reconnecting;
            if (m_callbacks.onStateChanged) {
                m_callbacks.onStateChanged(CameraState::Reconnecting, detail);
            }
            break;
        }
        if (flags & MF_SOURCE_READERF_ERROR) {
            rememberFailedMode(m_activeFormat);
            m_state = CameraState::Reconnecting;
            if (m_callbacks.onStateChanged) {
                m_callbacks.onStateChanged(CameraState::Reconnecting, "Source reader error");
            }
            break;
        }
        if (flags & (MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED |
                     MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)) {
            ComPtr<IMFMediaType> changed;
            if (SUCCEEDED(reader->GetCurrentMediaType(stream, &changed))) {
                MFGetAttributeSize(changed.Get(), MF_MT_FRAME_SIZE, &w, &h);
                m_activeFormat.resolution = {w, h};
            }
        }
        if (!sample) continue; // stream tick without data

        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->GetBufferByIndex(0, &buffer))) continue;

        Frame frame;
        frame.id = ++framesPublished;
        frame.timestamp = sampleTime100ns > 0
                              ? static_cast<uint64_t>(sampleTime100ns / 10)
                              : static_cast<uint64_t>(
                                    std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count());
        frame.width = m_activeFormat.resolution.width;
        frame.height = m_activeFormat.resolution.height;
        frame.format = PixelFormat::NV12;
        frame.metadata.frameId = frame.id;
        frame.metadata.timestampUs = frame.timestamp;
        frame.metadata.sourceFps = static_cast<uint32_t>(m_activeFormat.fps() + 0.5);
        frame.metadata.sourceName = "MF camera";

        // Preferred path: GPU-resident NV12. Decoder outputs are array
        // slices without SRV binding, so copy into a pooled texture.
        ComPtr<IMFDXGIBuffer> dxgiBuffer;
        if (texturePool && SUCCEEDED(buffer.As(&dxgiBuffer))) {
            ComPtr<ID3D11Texture2D> decoded;
            UINT subresource = 0;
            if (SUCCEEDED(dxgiBuffer->GetResource(IID_PPV_ARGS(decoded.GetAddressOf()))) &&
                SUCCEEDED(dxgiBuffer->GetSubresourceIndex(&subresource))) {
                GpuTextureRef pooled = texturePool->acquire(
                    frame.width, frame.height, PixelFormat::NV12,
                    static_cast<uint8_t>(TextureBind::ShaderResource));
                if (pooled) {
                    ComPtr<ID3D11DeviceContext> context;
                    device->GetImmediateContext(&context);
                    auto* dst = static_cast<ID3D11Texture2D*>(pooled->native());
                    context->CopySubresourceRegion(dst, 0, 0, 0, 0, decoded.Get(), subresource,
                                                   nullptr);
                    context->Flush();

                    if (!pooled->userData()) {
                        pooled->setUserData(new gfx::D3D11TextureViews{});
                        if (!viewFactory.createViews(*pooled)) {
                            pooled->setUserData(nullptr);
                            pooled.reset();
                        }
                    }
                    if (pooled) {
                        frame.texture.ref = pooled;
                        frame.texture.subresource = 0;
                        frame.metadata.gpuResident = true;
                        if (m_callbacks.onFrameReady) m_callbacks.onFrameReady(std::move(frame));
                        continue;
                    }
                }
                HAOCAM_LOG_WARN(kCategory, "GPU frame copy failed; using CPU path");
            }
        }

        // Fallback path: CPU buffer (software conversion output).
        BYTE* data = nullptr;
        DWORD maxLength = 0, currentLength = 0;
        if (SUCCEEDED(buffer->Lock(&data, &maxLength, &currentLength)) && data) {
            auto cpu = std::make_shared<FrameBuffer>();
            cpu->allocateNV12(frame.width, frame.height);
            const size_t copySize = std::min<size_t>(currentLength, cpu->data.size());
            std::memcpy(cpu->data.data(), data, copySize);
            buffer->Unlock();
            frame.cpuBuffer = cpu;
            frame.metadata.strideY = cpu->strideY;
            frame.metadata.strideUV = cpu->strideUV;
            frame.metadata.gpuResident = false;
            if (m_callbacks.onFrameReady) m_callbacks.onFrameReady(std::move(frame));
        } else {
            HAOCAM_LOG_WARN(kCategory, "Sample buffer lock failed; frame dropped");
        }
    }

    // Release COM objects on the thread that created them.
    texturePool.reset();
    reader.Reset();
    mediaSource.Reset();
    activation.Reset();
    dxgiManager.Reset();
    device.Reset();

    m_state = CameraState::Idle;
}

} // namespace haocam
