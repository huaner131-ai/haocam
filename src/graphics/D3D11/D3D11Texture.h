#pragma once

// D3D11 texture helpers: shader resource views (including the two-plane
// NV12 views) and the pooled ITexturePool implementation.

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>

#include "frame/TexturePool.h"

struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;

namespace haocam::gfx {

// Per-texture backend scratch data stored in GpuTextureStorage::userData().
struct D3D11TextureViews {
    ID3D11ShaderResourceView* srvPlane0 = nullptr; // Y or RGBA
    ID3D11ShaderResourceView* srvPlane1 = nullptr; // UV (NV12 only)
    ID3D11RenderTargetView* rtv = nullptr;         // render targets only
};

class D3D11TextureFactory {
public:
    explicit D3D11TextureFactory(ID3D11Device* device) : m_device(device) {}

    // Creates views for an existing texture described by `storage`.
    bool createViews(GpuTextureStorage& storage);

private:
    ID3D11Device* m_device = nullptr;
};

// Texture pool with reuse across frames (GPU principle #4).
//
// Lifetime: the pool is meant to be held through std::shared_ptr; recycled
// textures keep the pool alive through a shared_from_this captured in the
// recycle closure, so outstanding frames can never dangle even if their
// owner shuts the pool down early.
class D3D11TexturePool final : public ITexturePool,
                               public std::enable_shared_from_this<D3D11TexturePool> {
public:
    static std::shared_ptr<D3D11TexturePool> create(ID3D11Device* device);

    GpuTextureRef acquire(uint32_t width, uint32_t height, PixelFormat format,
                          uint8_t bindFlags) override;
    size_t inFlightCount() const override { return m_inFlight.load(); }
    size_t pooledCount() const override;

private:
    explicit D3D11TexturePool(ID3D11Device* device);

    struct Entry {
        uint32_t width = 0;
        uint32_t height = 0;
        PixelFormat format = PixelFormat::Unknown;
        uint8_t bindFlags = 0;
        std::shared_ptr<ID3D11Texture2D> texture; // custom deleter -> Release()
        bool inUse = false;
    };

    std::shared_ptr<ID3D11Texture2D> createTexture(uint32_t width, uint32_t height,
                                                   PixelFormat format, uint8_t bindFlags);
    GpuTextureRef wrapEntry(std::deque<Entry>::iterator entryIt);

    ID3D11Device* m_device = nullptr;
    mutable std::mutex m_mutex;
    std::deque<Entry> m_entries; // deque: stable references for append-only growth
    std::atomic<size_t> m_inFlight{0};
};

} // namespace haocam::gfx
