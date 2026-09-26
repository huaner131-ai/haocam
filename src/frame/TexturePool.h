#pragma once

// Frame shell + CPU buffer pooling and the backend GPU texture pool
// interface. Pools exist to avoid per-frame allocations (GPU principle #4).

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stack>
#include <vector>

#include "frame/FrameBuffer.h"
#include "frame/GPUTexture.h"

namespace haocam {

// Recycles CPU frame buffers for the fallback/synthetic source paths.
class FrameBufferPool {
public:
    using Ptr = std::shared_ptr<FrameBuffer>;

    explicit FrameBufferPool(size_t maxIdle = 8) : m_maxIdle(maxIdle) {}

    // Returns a buffer of the requested size. The returned shared_ptr is the
    // single owner; when it dies the buffer returns to the pool (or is freed
    // if the pool already holds enough idle buffers).
    Ptr acquireNV12(uint32_t width, uint32_t height) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::shared_ptr<FrameBuffer> owner;
        for (auto it = m_idle.begin(); it != m_idle.end(); ++it) {
            if ((*it)->width == width && (*it)->height == height) {
                owner = *it;
                m_idle.erase(it);
                break;
            }
        }
        if (!owner) {
            owner = std::make_shared<FrameBuffer>();
            owner->allocateNV12(width, height);
        }
        FrameBuffer* raw = owner.get();
        return Ptr(raw, [this, owner = std::move(owner)](FrameBuffer*) mutable {
            releaseOwner(std::move(owner));
        });
    }

    size_t idleCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_idle.size();
    }

private:
    void releaseOwner(std::shared_ptr<FrameBuffer> owner) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_idle.size() < m_maxIdle) {
            m_idle.push_back(std::move(owner));
        }
        // else: owner dies here and the buffer is freed.
    }

    mutable std::mutex m_mutex;
    std::vector<Ptr> m_idle;
    size_t m_maxIdle;
};

// Backend-implemented texture pool (D3D11 implementation lives in
// src/graphics/D3D11/D3D11TexturePool). Keeps a small set of reusable
// textures keyed by size/format/bind flags.
class ITexturePool {
public:
    virtual ~ITexturePool() = default;

    // Returns a GPU texture with the requested properties. The returned
    // reference automatically goes back to the pool on release.
    virtual GpuTextureRef acquire(uint32_t width, uint32_t height, PixelFormat format,
                                  uint8_t bindFlags) = 0;

    // Textures currently loaned out (diagnostics).
    virtual size_t inFlightCount() const = 0;
    virtual size_t pooledCount() const = 0;
};

} // namespace haocam
