#include "graphics/compositor/Compositor.h"

#include <chrono>
#include <cstring>

#include "core/logging/Logger.h"
#include "graphics/D3D11/D3D11Shader.h"
#include "graphics/D3D11/D3D11Texture.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

// Embedded shaders (generated at build time by cmake/EmbedFile.cmake).
#include <fullscreen_vs.hlsl.h>
#include <color_ps.hlsl.h>
#include <composite_ps.hlsl.h>

namespace haocam {

using Microsoft::WRL::ComPtr;

namespace {
constexpr const char* kCategory = "gpu";
constexpr int kRingSlots = 3;

struct MirrorConstants {
    float mirrorX = 0.0f;
    float mirrorY = 0.0f;
    float padding[2] = {0.0f, 0.0f};
};

struct ColorConstants {
    float brightness = 0.0f;
    float contrast = 0.0f;
    float saturation = 0.0f;
    float padding = 0.0f;
};
} // namespace

struct Compositor::Pipeline {
    std::shared_ptr<gfx::D3D11TexturePool> pool;
    gfx::D3D11SamplerCache samplers;

    gfx::D3D11ShaderProgram colorProgram;    // NV12 -> BGRA + adjustments + mirror
    gfx::D3D11ShaderProgram compositeProgram; // processed -> final

    // GPU timestamp queries (one frame of latency).
    ComPtr<ID3D11Query> disjointPrev;
    ComPtr<ID3D11Query> startPrev;
    ComPtr<ID3D11Query> endPrev;
    bool queryInFlight = false;
};

Compositor::Compositor() = default;

Compositor::~Compositor() { shutdown(); }

bool Compositor::initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (!device || !context) return false;
    shutdown();

    m_device = device;
    m_context = context;
    auto* pipeline = new Pipeline();
    m_pipeline.reset(pipeline);

    pipeline->pool = gfx::D3D11TexturePool::create(device);
    if (!pipeline->pool) {
        HAOCAM_LOG_ERROR(kCategory, "Compositor texture pool creation failed");
        m_device = nullptr;
        m_context = nullptr;
        m_pipeline.reset();
        return false;
    }

    if (!pipeline->colorProgram.loadFromEmbeddedSource(
            device, shaders::k_fullscreen_vs_hlsl, shaders::k_fullscreen_vs_hlsl_size,
            shaders::k_color_ps_hlsl, shaders::k_color_ps_hlsl_size) ||
        !pipeline->compositeProgram.loadFromEmbeddedSource(
            device, shaders::k_fullscreen_vs_hlsl, shaders::k_fullscreen_vs_hlsl_size,
            shaders::k_composite_ps_hlsl, shaders::k_composite_ps_hlsl_size)) {
        HAOCAM_LOG_ERROR(kCategory, "Compositor shader compilation failed");
        m_device = nullptr;
        m_context = nullptr;
        m_pipeline.reset();
        return false;
    }

    HAOCAM_LOG_INFO(kCategory, "GPU compositor initialized (2 passes, D3D11)");
    return true;
}

void Compositor::shutdown() {
    m_pipeline.reset();
    {
        std::lock_guard<std::mutex> lock(m_outputMutex);
        for (int i = 0; i < kRingSlots; ++i) {
            m_outputRing[i] = CompositorOutput{};
            m_outputIds[i] = 0;
        }
    }
    m_latestFrameId.store(0, std::memory_order_release);
    m_device = nullptr;
    m_context = nullptr;
}

size_t Compositor::pooledTextures() const {
    return m_pipeline ? m_pipeline->pool->pooledCount() : 0;
}

void Compositor::process(Frame& frame) {
    if (!m_pipeline || !frame.isValid()) return;
    auto* pipeline = m_pipeline.get();
    const auto cpuStart = std::chrono::steady_clock::now();

    const uint32_t width = frame.width;
    const uint32_t height = frame.height;

    // ---- Begin GPU timing (reads the previous frame's result) ----
    if (pipeline->queryInFlight && pipeline->disjointPrev) {
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
        if (m_context->GetData(pipeline->disjointPrev.Get(), &disjoint, sizeof(disjoint), 0) == S_OK &&
            disjoint.Frequency > 0 && !disjoint.Disjoint) {
            UINT64 start = 0, end = 0;
            m_context->GetData(pipeline->startPrev.Get(), &start, sizeof(start), 0);
            m_context->GetData(pipeline->endPrev.Get(), &end, sizeof(end), 0);
            m_gpuTimeMs.store(static_cast<double>(end - start) * 1000.0 /
                                  static_cast<double>(disjoint.Frequency),
                              std::memory_order_relaxed);
        }
        pipeline->queryInFlight = false;
    }

    ComPtr<ID3D11Query> disjoint, tsStart, tsEnd;
    {
        D3D11_QUERY_DESC qd{};
        qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        if (SUCCEEDED(m_device->CreateQuery(&qd, disjoint.GetAddressOf()))) {
            qd.Query = D3D11_QUERY_TIMESTAMP;
            if (SUCCEEDED(m_device->CreateQuery(&qd, tsStart.GetAddressOf())) &&
                SUCCEEDED(m_device->CreateQuery(&qd, tsEnd.GetAddressOf()))) {
                m_context->Begin(disjoint.Get());
                m_context->End(tsStart.Get());
            }
        }
    }

    ID3D11SamplerState* sampler = pipeline->samplers.linearClamp(m_device);

    // ---- Resolve input as shader resource views ----
    void* srvY = nullptr;
    void* srvUV = nullptr;
    GpuTextureRef uploadTexture;

    if (frame.isGpu()) {
        auto* views = static_cast<gfx::D3D11TextureViews*>(frame.texture.ref->userData());
        if (!views || !views->srvPlane0) {
            // Views missing: rebuild them once for this storage.
            gfx::D3D11TextureFactory factory(m_device);
            if (!frame.texture.ref->userData()) {
                frame.texture.ref->setUserData(new gfx::D3D11TextureViews{});
                if (!factory.createViews(*frame.texture.ref)) {
                    frame.texture.ref->setUserData(nullptr);
                    return;
                }
                views = static_cast<gfx::D3D11TextureViews*>(frame.texture.ref->userData());
            } else {
                return;
            }
        }
        srvY = views->srvPlane0;
        srvUV = views->srvPlane1;
    } else if (frame.cpuBuffer) {
        // CPU fallback: one UpdateSubresource into a pooled NV12 texture
        // (NV12 is a single contiguous buffer: Y plane + UV plane), then the
        // pipeline continues purely on the GPU.
        uploadTexture = pipeline->pool->acquire(
            width, height, PixelFormat::NV12, static_cast<uint8_t>(TextureBind::ShaderResource));
        if (!uploadTexture) return;
        auto* texture = static_cast<ID3D11Texture2D*>(uploadTexture->native());
        m_context->UpdateSubresource(texture, 0, nullptr, frame.cpuBuffer->data.data(),
                                     frame.cpuBuffer->strideY, height * 3 / 2);

        gfx::D3D11TextureFactory factory(m_device);
        if (!uploadTexture->userData()) {
            uploadTexture->setUserData(new gfx::D3D11TextureViews{});
            if (!factory.createViews(*uploadTexture)) {
                uploadTexture->setUserData(nullptr);
                return;
            }
        }
        auto* views = static_cast<gfx::D3D11TextureViews*>(uploadTexture->userData());
        srvY = views->srvPlane0;
        srvUV = views->srvPlane1;
    }
    if (!srvY) return;

    // ---- Pass 1: color convert (+ adjustments, mirror) ----
    GpuTextureRef processed = pipeline->pool->acquire(
        width, height, PixelFormat::BGRA8,
        static_cast<uint8_t>(TextureBind::ShaderResource) |
            static_cast<uint8_t>(TextureBind::RenderTarget));
    if (!processed) return;
    auto* processedViews = static_cast<gfx::D3D11TextureViews*>(processed->userData());

    {
        MirrorConstants mirror;
        mirror.mirrorX = frame.metadata.mirrored ? 1.0f : 0.0f;
        pipeline->colorProgram.setVertexConstants(m_context, &mirror, sizeof(mirror));

        ColorConstants color;
        color.brightness = m_adjustments.brightness;
        color.contrast = m_adjustments.contrast;
        color.saturation = m_adjustments.saturation;
        pipeline->colorProgram.setPixelConstants(m_context, &color, sizeof(color));

        ID3D11RenderTargetView* rtv = processedViews->rtv;
        m_context->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT viewport{0, 0, static_cast<float>(width), static_cast<float>(height),
                                0.0f, 1.0f};
        m_context->RSSetViewports(1, &viewport);

        ID3D11Buffer* vsCb = pipeline->colorProgram.vertexConstants();
        ID3D11Buffer* psCb = pipeline->colorProgram.pixelConstants();
        m_context->VSSetConstantBuffers(0, 1, &vsCb);
        m_context->PSSetConstantBuffers(0, 1, &psCb);
        ID3D11ShaderResourceView* srvs[2] = {
            static_cast<ID3D11ShaderResourceView*>(srvY),
            static_cast<ID3D11ShaderResourceView*>(srvUV),
        };
        m_context->PSSetShaderResources(0, 2, srvs);
        m_context->PSSetSamplers(0, 1, &sampler);
        m_context->VSSetShader(pipeline->colorProgram.vertexShader(), nullptr, 0);
        m_context->PSSetShader(pipeline->colorProgram.pixelShader(), nullptr, 0);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->Draw(3, 0);
    }

    // ---- Pass 2: composite into the final ring texture ----
    GpuTextureRef finalTexture = pipeline->pool->acquire(
        width, height, PixelFormat::BGRA8,
        static_cast<uint8_t>(TextureBind::ShaderResource) |
            static_cast<uint8_t>(TextureBind::RenderTarget));
    if (!finalTexture) return;
    auto* finalViews = static_cast<gfx::D3D11TextureViews*>(finalTexture->userData());

    {
        static const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        ID3D11RenderTargetView* rtv = finalViews->rtv;
        m_context->ClearRenderTargetView(rtv, black);
        m_context->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT viewport{0, 0, static_cast<float>(width), static_cast<float>(height),
                                0.0f, 1.0f};
        m_context->RSSetViewports(1, &viewport);

        ID3D11ShaderResourceView* srv = processedViews->srvPlane0;
        m_context->PSSetShaderResources(0, 1, &srv);
        m_context->PSSetSamplers(0, 1, &sampler);
        m_context->VSSetShader(pipeline->compositeProgram.vertexShader(), nullptr, 0);
        m_context->PSSetShader(pipeline->compositeProgram.pixelShader(), nullptr, 0);
        m_context->Draw(3, 0);
    }

    m_context->End(tsEnd.Get());
    m_context->End(disjoint.Get());
    pipeline->disjointPrev = disjoint;
    pipeline->startPrev = tsStart;
    pipeline->endPrev = tsEnd;
    pipeline->queryInFlight = true;
    m_context->Flush(); // submit now so consumers on other threads see the frame

    // ---- Publish into the ring (wait for UI consumption of the slot) ----
    {
        std::unique_lock<std::mutex> lock(m_outputMutex);
        const int slot = m_ringIndex;
        if (m_outputIds[slot] > m_consumedId) {
            // The UI has not drawn this slot yet; give it one vsync, then a
            // bounded grace period before we proceed (never deadlock).
            m_outputReady.wait_for(lock, std::chrono::milliseconds(8), [&] {
                return m_consumedId >= m_outputIds[slot];
            });
        }
        m_outputRing[slot].texture = finalTexture;
        m_outputRing[slot].frameId = frame.id;
        m_outputRing[slot].width = width;
        m_outputRing[slot].height = height;
        m_outputIds[slot] = frame.id;
        m_latestFrameId.store(frame.id, std::memory_order_release);
        m_ringIndex = (slot + 1) % kRingSlots;
    }

    // ---- Stats ----
    const auto cpuEnd = std::chrono::steady_clock::now();
    const double cpuMs = std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count();
    m_cpuTimeMs.store(cpuMs, std::memory_order_relaxed);

    const auto now = std::chrono::steady_clock::now();
    if (m_lastProcess.time_since_epoch().count() > 0) {
        const double seconds = std::chrono::duration<double>(now - m_lastProcess).count();
        if (seconds > 0.0) {
            const double instant = 1.0 / seconds;
            const double smoothed = m_processFps.load(std::memory_order_relaxed);
            m_processFps.store(smoothed > 0 ? smoothed * 0.9 + instant * 0.1 : instant,
                               std::memory_order_relaxed);
        }
    }
    m_lastProcess = now;
    m_renderGraph.stats(gfx::RenderPassId::ColorConvert).invocations++;
    m_renderGraph.stats(gfx::RenderPassId::Composite).invocations++;
}

CompositorOutput Compositor::latestOutput() {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    const uint64_t latest = m_latestFrameId.load(std::memory_order_acquire);
    for (int i = 0; i < kRingSlots; ++i) {
        if (m_outputIds[i] == latest) {
            return m_outputRing[i];
        }
    }
    return {};
}

void Compositor::notifyOutputConsumed(uint64_t frameId) {
    {
        std::lock_guard<std::mutex> lock(m_outputMutex);
        if (frameId > m_consumedId) m_consumedId = frameId;
    }
    m_outputReady.notify_all();
}

} // namespace haocam
