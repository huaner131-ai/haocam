#include "graphics/D3D11/GpuFrameCopier.h"

#include <chrono>
#include <cstring>

#include "core/logging/Logger.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace haocam::gfx {

using Microsoft::WRL::ComPtr;

namespace {
constexpr const char* kCategory = "gpu";

uint64_t nowUs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}
} // namespace

std::unique_ptr<GpuFrameCopier> GpuFrameCopier::create(ID3D11Device* device) {
    if (!device) return nullptr;
    auto* copier = new GpuFrameCopier();
    copier->m_device = device;
    return std::unique_ptr<GpuFrameCopier>(copier);
}

GpuFrameCopier::~GpuFrameCopier() {
    for (Staging& slot : m_ring) {
        if (slot.texture) {
            static_cast<ID3D11Texture2D*>(slot.texture)->Release();
            slot.texture = nullptr;
        }
    }
}

bool GpuFrameCopier::readBGRA(const GpuTextureRef& src, std::vector<uint8_t>& dst) {
    if (!m_device || !src || !src->native()) return false;

    auto* texture = static_cast<ID3D11Texture2D*>(src->native());
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        HAOCAM_LOG_WARN(kCategory, "GpuFrameCopier: unexpected source format 0x{:08X}",
                        static_cast<unsigned>(desc.Format));
        return false;
    }
    if (desc.Width != m_width || desc.Height != m_height) {
        // Resolution changed: recreate the staging ring.
        for (Staging& slot : m_ring) {
            if (slot.texture) {
                static_cast<ID3D11Texture2D*>(slot.texture)->Release();
                slot.texture = nullptr;
            }
            slot.pending = false;
        }
        m_width = desc.Width;
        m_height = desc.Height;
    }

    ComPtr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);
    if (!context) return false;

    Staging& slot = m_ring[m_next];
    m_next = (m_next + 1) % 2;

    const uint64_t start = nowUs();
    if (!slot.texture) {
        D3D11_TEXTURE2D_DESC stagingDesc = desc;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.MipLevels = 1;
        stagingDesc.ArraySize = 1;
        stagingDesc.SampleDesc.Count = 1;
        if (FAILED(m_device->CreateTexture2D(&stagingDesc, nullptr,
                       reinterpret_cast<ID3D11Texture2D**>(&slot.texture)))) {
            HAOCAM_LOG_ERROR(kCategory, "GpuFrameCopier: staging texture creation failed");
            return false;
        }
    }

    auto* staging = static_cast<ID3D11Texture2D*>(slot.texture);
    context->CopyResource(staging, texture);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        return false;
    }
    dst.resize(static_cast<size_t>(m_width) * m_height * 4);
    const size_t rowBytes = static_cast<size_t>(m_width) * 4;
    if (mapped.RowPitch == rowBytes) {
        std::memcpy(dst.data(), mapped.pData, dst.size());
    } else {
        const uint8_t* srcBytes = static_cast<const uint8_t*>(mapped.pData);
        for (UINT row = 0; row < m_height; ++row) {
            std::memcpy(dst.data() + row * rowBytes, srcBytes + row * mapped.RowPitch,
                        rowBytes);
        }
    }
    context->Unmap(staging, 0);

    m_lastReadMs = static_cast<double>(nowUs() - start) / 1000.0;
    return true;
}

GpuTextureRef GpuFrameCopier::uploadBGRA(ITexturePool& pool, const uint8_t* data,
                                         uint32_t width, uint32_t height) {
    if (!m_device || !data || width == 0 || height == 0) return nullptr;
    const uint64_t start = nowUs();

    GpuTextureRef texture =
        pool.acquire(width, height, PixelFormat::BGRA8,
                     static_cast<uint8_t>(TextureBind::ShaderResource));
    if (!texture) return nullptr;

    ComPtr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);
    if (!context) return nullptr;

    auto* dst = static_cast<ID3D11Texture2D*>(texture->native());
    context->UpdateSubresource(dst, 0, nullptr, data, width * 4, 0);

    m_lastUploadMs = static_cast<double>(nowUs() - start) / 1000.0;
    return texture;
}

} // namespace haocam::gfx
