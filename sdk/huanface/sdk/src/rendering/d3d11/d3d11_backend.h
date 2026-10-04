/**
 * HuanFace D3D11 Backend — Phase 3 P0
 * Minimal implementation for Windows x64
 * On non-Windows, this file is not compiled
 */

#pragma once

#ifdef _WIN32

#include "../render_backend.h"
#include "../resource_pool.h"
#include "../../core/profiler.h"
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <string>

namespace huanface {

using Microsoft::WRL::ComPtr;

// Production D3D11: timestamp query for GPU timing
struct GPUTimestampQuery {
    ComPtr<ID3D11Query> disjointQuery;
    ComPtr<ID3D11Query> startQuery;
    ComPtr<ID3D11Query> endQuery;
    bool isStarted=false;
    double gpuMs=0;
};

class D3D11Texture : public IGpuTexture {
public:
    D3D11Texture(int w, int h, HFFormat fmt, ComPtr<ID3D11Texture2D> tex, ComPtr<ID3D11ShaderResourceView> srv)
        : width(w), height(h), format(fmt), texture(tex), srv(srv) {}
    int GetWidth() const override { return width; }
    int GetHeight() const override { return height; }
    HFFormat GetFormat() const override { return format; }
    void* GetNativeHandle() const override { return texture.Get(); }
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;
    int width, height;
    HFFormat format;
};

class D3D11RenderTarget : public IRenderTarget {
public:
    D3D11RenderTarget(int w, int h, HFFormat fmt, ComPtr<ID3D11Texture2D> tex, ComPtr<ID3D11RenderTargetView> rtv, ComPtr<ID3D11ShaderResourceView> srv)
        : width(w), height(h), format(fmt), texture(tex), rtv(rtv), srv(srv) {
        // Create wrapper texture that shares same underlying texture but with SRV
        gpuTexture = new D3D11Texture(w, h, fmt, tex, srv);
    }
    ~D3D11RenderTarget() { delete gpuTexture; }
    IGpuTexture* GetTexture() override { return gpuTexture; }
    void Bind() override {}
    void Unbind() override {}
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
    D3D11Texture* gpuTexture;
    int width, height;
    HFFormat format;
};

class D3D11Shader : public IShader {
public:
    void SetUniform(const std::string& name, const Uniform& uniform) override {
        uniforms[name] = uniform;
    }
    void SetTexture(const std::string& name, IGpuTexture* texture) override {
        textures[name] = texture;
    }
    std::map<std::string, Uniform> uniforms;
    std::map<std::string, IGpuTexture*> textures;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11Buffer> constantBuffer;
};

class D3D11Mesh : public IMesh {
public:
    D3D11Mesh(const std::vector<float>& verts, const std::vector<int>& inds) : vertices(verts), indices(inds) {}
    void* GetNativeHandle() const override { return nullptr; }
    int GetVertexCount() const override { return (int)vertices.size() / 3; }
    int GetIndexCount() const override { return (int)indices.size(); }
    std::vector<float> vertices;
    std::vector<int> indices;
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
};

class D3D11Backend : public IRenderBackend {
public:
    D3D11Backend();
    ~D3D11Backend();

    HFResult Init(void* windowHandle = nullptr) override;
    void Shutdown() override;
    bool IsInitialized() const override { return initialized; }
    HFRenderBackendType GetType() const override { return HF_RENDER_BACKEND_D3D11; }

    IGpuTexture* CreateTexture(int width, int height, HFFormat format, const void* data) override;
    IGpuTexture* CreateTextureFromFile(const std::string& path) override;
    // Phase 4: real PNG memory -> Texture2D SRV
    IGpuTexture* CreateTextureFromMemory(const uint8_t* pngData, size_t pngSize);
    void DestroyTexture(IGpuTexture* texture) override;

    IRenderTarget* CreateRenderTarget(int width, int height, HFFormat format = HF_FORMAT_RGBA8) override;
    void DestroyRenderTarget(IRenderTarget* rt) override;
    void SetRenderTarget(IRenderTarget* rt) override;

    void Clear(float r, float g, float b, float a) override;

    IShader* CreateShader(const std::string& vsSrc, const std::string& fsSrc) override;
    IShader* CreateShaderFromFile(const std::string& vsPath, const std::string& fsPath) override;
    // Phase 9: compile specific entry point (e.g. PSContrast from beauty_adjustment.hlsl, PSEyebrow from makeup_eye.hlsl)
    IShader* CreateShaderFromFileWithEntry(const std::string& vsPath, const std::string& fsPath, const std::string& psEntry);
    void DestroyShader(IShader* shader) override;

    IMesh* CreateMesh(const std::vector<float>& vertices, const std::vector<int>& indices) override;
    void DestroyMesh(IMesh* mesh) override;

    void DrawMesh(IMesh* mesh, IShader* shader, const std::map<std::string, Uniform>& uniforms) override;
    void Blit(IGpuTexture* src, IRenderTarget* dst) override;
    void Present() override;

    void SetBlendMode(HFRenderBlendMode mode) override;
    void SetMakeupBlendMode(HFBlendMode mode) override;
    void SetDepthTest(bool enable) override;
    void SetCullMode(bool enable) override;

public:
    // Phase 8 Production: Adapter info, feature level, profiling
    struct AdapterInfo {
        std::string description;
        std::string vendor;
        size_t dedicatedVideoMemory=0;
        size_t dedicatedSystemMemory=0;
        size_t sharedSystemMemory=0;
        D3D_FEATURE_LEVEL featureLevel=D3D_FEATURE_LEVEL_11_0;
        std::string featureLevelStr;
        bool isWarp=false;
    };
    AdapterInfo GetAdapterInfo() const { return adapterInfo_; }
    D3D_FEATURE_LEVEL GetFeatureLevel() const { return featureLevel_; }
    ID3D11Device* GetDevice() const { return device.Get(); }
    ID3D11DeviceContext* GetContext() const { return context.Get(); }

    // Phase 8: GPU timing via timestamp queries
    HFResult CreateTimestampQueries(GPUTimestampQuery& query);
    bool BeginGPUTimestamp(GPUTimestampQuery& query);
    bool EndGPUTimestamp(GPUTimestampQuery& query);
    double GetGPUTimestampMs(GPUTimestampQuery& query);

    // Phase 8: Resource pooling — acquire/release
    IRenderTarget* AcquirePooledRenderTarget(int width, int height, HFFormat format);
    void ReleasePooledRenderTarget(IRenderTarget* rt);
    IGpuTexture* AcquirePooledTexture(int width, int height, HFFormat format);
    void ReleasePooledTexture(IGpuTexture* tex);
    void OnResolutionChanged();
    void BeginFrame();
    void EndFrame();

    // Phase 8: Error recovery
    bool IsDeviceLost();
    HFResult HandleDeviceLost();

private:
    bool initialized = false;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11RenderTargetView> backBufferRTV;
    IRenderTarget* currentRT = nullptr;
    ComPtr<IDXGIFactory> dxgiFactory_;
    ComPtr<IDXGIAdapter> dxgiAdapter_;
    AdapterInfo adapterInfo_;
    D3D_FEATURE_LEVEL featureLevel_ = D3D_FEATURE_LEVEL_11_0;
    GPUResourcePool resourcePool_;
    int64_t frameCounter_=0;
    bool deviceLost_=false;

    DXGI_FORMAT ToDXGIFormat(HFFormat fmt);
    HFResult CreateDeviceAndContext(void* windowHandle);
    void QueryAdapterInfo();
    std::string FeatureLevelToString(D3D_FEATURE_LEVEL level);
};

} // namespace huanface

#endif // _WIN32
