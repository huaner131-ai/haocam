#pragma once

// GPU-resident texture handle. The Frame model keeps pixels on the GPU; this
// header stays backend-neutral: the native handle (e.g. ID3D11Texture2D*) is
// owned by the graphics layer through a recycling shared_ptr.

#include <cstdint>
#include <functional>
#include <memory>

#include "frame/FrameMetadata.h"
#include "graphics/RenderTypes.h"

namespace haocam {

// Ref-counted GPU texture storage. `recycle` returns the texture to the
// owning TexturePool (or destroys it) when the last reference dies.
class GpuTextureStorage {
public:
    GpuTextureStorage(void* native, GpuBackend backend, PixelFormat format, uint32_t width,
                      uint32_t height, std::function<void(GpuTextureStorage*)> recycle)
        : m_native(native),
          m_backend(backend),
          m_format(format),
          m_width(width),
          m_height(height),
          m_recycle(std::move(recycle)) {}

    ~GpuTextureStorage() {
        if (m_recycle) m_recycle(this);
    }

    GpuTextureStorage(const GpuTextureStorage&) = delete;
    GpuTextureStorage& operator=(const GpuTextureStorage&) = delete;

    void* native() const { return m_native; }
    GpuBackend backend() const { return m_backend; }
    PixelFormat format() const { return m_format; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    void* userData() const { return m_userData; }
    void setUserData(void* data) { m_userData = data; }

private:
    void* m_native = nullptr;
    GpuBackend m_backend = GpuBackend::None;
    PixelFormat m_format = PixelFormat::Unknown;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    void* m_userData = nullptr; // backend scratch (e.g. SRVs)
    std::function<void(GpuTextureStorage*)> m_recycle;
};

using GpuTextureRef = std::shared_ptr<GpuTextureStorage>;

// The Frame-visible texture view as described in the architecture spec.
struct GPUTexture {
    GpuTextureRef ref;
    uint32_t subresource = 0; // NV12 decoder array slice, if any

    bool valid() const { return ref != nullptr && ref->native() != nullptr; }
    uint32_t width() const { return valid() ? ref->width() : 0; }
    uint32_t height() const { return valid() ? ref->height() : 0; }
    PixelFormat format() const { return valid() ? ref->format() : PixelFormat::Unknown; }
};

} // namespace haocam
