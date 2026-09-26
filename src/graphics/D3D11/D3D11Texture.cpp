#include "graphics/D3D11/D3D11Texture.h"

#include "core/logging/Logger.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace haocam::gfx {

using Microsoft::WRL::ComPtr;

namespace {
constexpr const char* kCategory = "gpu";

DXGI_FORMAT typedFormat(PixelFormat format) {
    switch (format) {
        case PixelFormat::NV12: return DXGI_FORMAT_NV12;
        case PixelFormat::BGRA8: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case PixelFormat::RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::Unknown: break;
    }
    return DXGI_FORMAT_UNKNOWN;
}
} // namespace

bool D3D11TextureFactory::createViews(GpuTextureStorage& storage) {
    auto* texture = static_cast<ID3D11Texture2D*>(storage.native());
    auto* views = static_cast<D3D11TextureViews*>(storage.userData());
    if (!texture || !views) return false;

    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    const DXGI_FORMAT base = desc.Format;

    if (base == DXGI_FORMAT_NV12) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv0{};
        srv0.Format = DXGI_FORMAT_R8_UNORM;
        srv0.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv0.Texture2D.MipLevels = 1;
        if (FAILED(m_device->CreateShaderResourceView(texture, &srv0, &views->srvPlane0))) {
            return false;
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC srv1{};
        srv1.Format = DXGI_FORMAT_R8G8_UNORM;
        srv1.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv1.Texture2D.MipLevels = 1;
        if (FAILED(m_device->CreateShaderResourceView(texture, &srv1, &views->srvPlane1))) {
            return false;
        }
    } else {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = base;
        srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MipLevels = 1;
        if (FAILED(m_device->CreateShaderResourceView(texture, &srv, &views->srvPlane0))) {
            return false;
        }
    }

    if (desc.BindFlags & D3D11_BIND_RENDER_TARGET) {
        if (FAILED(m_device->CreateRenderTargetView(texture, nullptr, &views->rtv))) {
            return false;
        }
    }
    return true;
}

std::shared_ptr<D3D11TexturePool> D3D11TexturePool::create(ID3D11Device* device) {
    return std::shared_ptr<D3D11TexturePool>(new D3D11TexturePool(device));
}

D3D11TexturePool::D3D11TexturePool(ID3D11Device* device) : m_device(device) {}

std::shared_ptr<ID3D11Texture2D> D3D11TexturePool::createTexture(uint32_t width, uint32_t height,
                                                                 PixelFormat format,
                                                                 uint8_t bindFlags) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Format = typedFormat(format);
    if (hasBind(bindFlags, TextureBind::ShaderResource)) {
        desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (hasBind(bindFlags, TextureBind::RenderTarget)) {
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(m_device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf()))) {
        HAOCAM_LOG_ERROR(kCategory, "Pool texture creation failed ({}x{}, {})", width, height,
                         toString(format));
        return nullptr;
    }
    ID3D11Texture2D* raw = texture.Detach();
    return std::shared_ptr<ID3D11Texture2D>(raw, [](ID3D11Texture2D* t) {
        if (t) t->Release();
    });
}

GpuTextureRef D3D11TexturePool::acquire(uint32_t width, uint32_t height, PixelFormat format,
                                        uint8_t bindFlags) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (!it->inUse && it->format == format && it->bindFlags == bindFlags &&
            it->width == width && it->height == height && it->texture) {
            it->inUse = true;
            m_inFlight.fetch_add(1);
            return wrapEntry(it);
        }
    }

    Entry entry;
    entry.width = width;
    entry.height = height;
    entry.format = format;
    entry.bindFlags = bindFlags;
    entry.texture = createTexture(width, height, format, bindFlags);
    if (!entry.texture) return nullptr;
    entry.inUse = true;
    m_entries.push_back(std::move(entry));
    m_inFlight.fetch_add(1);
    return wrapEntry(std::prev(m_entries.end()));
}

GpuTextureRef D3D11TexturePool::wrapEntry(std::deque<Entry>::iterator entryIt) {
    Entry* entry = &*entryIt;
    auto self = shared_from_this(); // keeps the pool alive while frames live
    auto storage = std::make_shared<GpuTextureStorage>(
        entry->texture.get(), GpuBackend::D3D11, entry->format, entry->width, entry->height,
        [self, entry](GpuTextureStorage* s) {
            delete static_cast<D3D11TextureViews*>(s->userData());
            s->setUserData(nullptr);
            self->m_inFlight.fetch_sub(1);
            std::lock_guard<std::mutex> lock(self->m_mutex);
            entry->inUse = false; // texture returns to the pool
        });
    storage->setUserData(new D3D11TextureViews{});

    D3D11TextureFactory factory(m_device);
    if (!factory.createViews(*storage)) {
        HAOCAM_LOG_ERROR(kCategory, "Failed to create views for pooled texture");
        // Recycling storage releases views + returns the entry to idle.
        return nullptr;
    }
    return storage;
}

size_t D3D11TexturePool::pooledCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}

} // namespace haocam::gfx
