/**
 * HuanFace Null/Software Render Backend — Phase 3
 * For testing on Linux without D3D11, does CPU blit
 */

#pragma once
#include "render_backend.h"
#include <map>
#include <cstring>
#include <vector>

namespace huanface {

class NullTexture : public IGpuTexture {
public:
    NullTexture(int w, int h, HFFormat fmt) : width(w), height(h), format(fmt) {
        // Allocate CPU buffer for texture data
        int bpp = 4;
        if (fmt == HF_FORMAT_RGB8 || fmt == HF_FORMAT_BGR8) bpp = 3;
        else if (fmt == HF_FORMAT_R8) bpp = 1;
        else if (fmt == HF_FORMAT_R32F) bpp = 4;
        data.resize((size_t)w * h * bpp, 0);
    }
    int GetWidth() const override { return width; }
    int GetHeight() const override { return height; }
    HFFormat GetFormat() const override { return format; }
    void* GetNativeHandle() const override { return nullptr; }

    int width, height;
    HFFormat format;
    std::vector<uint8_t> data;
};

class NullRenderTarget : public IRenderTarget {
public:
    NullRenderTarget(int w, int h, HFFormat fmt) {
        texture = new NullTexture(w, h, fmt);
    }
    ~NullRenderTarget() { delete texture; }
    IGpuTexture* GetTexture() override { return texture; }
    void Bind() override {}
    void Unbind() override {}
    NullTexture* texture;
};

class NullShader : public IShader {
public:
    void SetUniform(const std::string& name, const Uniform& uniform) override {
        uniforms[name] = uniform;
    }
    void SetTexture(const std::string& name, IGpuTexture* texture) override {
        textures[name] = texture;
    }
    std::map<std::string, Uniform> uniforms;
    std::map<std::string, IGpuTexture*> textures;
};

class NullMesh : public IMesh {
public:
    NullMesh(const std::vector<float>& verts, const std::vector<int>& inds) : vertices(verts), indices(inds) {}
    void* GetNativeHandle() const override { return nullptr; }
    int GetVertexCount() const override { return (int)vertices.size() / 3; } // assume 3 floats per vertex for minimal
    int GetIndexCount() const override { return (int)indices.size(); }
    std::vector<float> vertices;
    std::vector<int> indices;
};

class NullRenderBackend : public IRenderBackend {
public:
    NullRenderBackend() : initialized(false), currentRT(nullptr) {}
    ~NullRenderBackend() { Shutdown(); }

    HFResult Init(void* windowHandle = nullptr) override {
        (void)windowHandle;
        initialized = true;
        return HF_RESULT_OK;
    }
    void Shutdown() override {
        // Clean up any remaining resources? For Phase 3 minimal, just mark uninitialized
        initialized = false;
        currentRT = nullptr;
    }
    bool IsInitialized() const override { return initialized; }
    HFRenderBackendType GetType() const override { return HF_RENDER_BACKEND_AUTO; }

    IGpuTexture* CreateTexture(int width, int height, HFFormat format, const void* data) override {
        if (width <=0 || height <=0) return nullptr;
        NullTexture* tex = new NullTexture(width, height, format);
        if (data) {
            size_t dataSize = tex->data.size();
            // Copy if data provided
            memcpy(tex->data.data(), data, std::min(dataSize, (size_t)width*height*4));
        }
        return tex;
    }
    IGpuTexture* CreateTextureFromFile(const std::string& path) override {
        (void)path;
        // For Phase 3, not implemented, return dummy 256x256
        return CreateTexture(256, 256, HF_FORMAT_RGBA8, nullptr);
    }
    void DestroyTexture(IGpuTexture* texture) override {
        delete static_cast<NullTexture*>(texture);
    }

    IRenderTarget* CreateRenderTarget(int width, int height, HFFormat format = HF_FORMAT_RGBA8) override {
        if (width <=0 || height <=0) return nullptr;
        return new NullRenderTarget(width, height, format);
    }
    void DestroyRenderTarget(IRenderTarget* rt) override {
        delete static_cast<NullRenderTarget*>(rt);
    }
    void SetRenderTarget(IRenderTarget* rt) override {
        currentRT = static_cast<NullRenderTarget*>(rt);
    }

    void Clear(float r, float g, float b, float a) override {
        if (!currentRT || !currentRT->texture) return;
        // Fill with color
        uint8_t cr = (uint8_t)(r * 255);
        uint8_t cg = (uint8_t)(g * 255);
        uint8_t cb = (uint8_t)(b * 255);
        uint8_t ca = (uint8_t)(a * 255);
        auto& data = currentRT->texture->data;
        for (size_t i=0; i+3 < data.size(); i+=4) {
            data[i+0] = cr;
            data[i+1] = cg;
            data[i+2] = cb;
            data[i+3] = ca;
        }
    }

    IShader* CreateShader(const std::string& vsSrc, const std::string& fsSrc) override {
        (void)vsSrc; (void)fsSrc;
        return new NullShader();
    }
    IShader* CreateShaderFromFile(const std::string& vsPath, const std::string& fsPath) override {
        (void)vsPath; (void)fsPath;
        return new NullShader();
    }
    void DestroyShader(IShader* shader) override {
        delete static_cast<NullShader*>(shader);
    }

    IMesh* CreateMesh(const std::vector<float>& vertices, const std::vector<int>& indices) override {
        return new NullMesh(vertices, indices);
    }
    void DestroyMesh(IMesh* mesh) override {
        delete static_cast<NullMesh*>(mesh);
    }

    void DrawMesh(IMesh* mesh, IShader* shader, const std::map<std::string, Uniform>& uniforms) override {
        (void)mesh; (void)shader; (void)uniforms;
        // No-op for null backend, but could simulate
        if (!currentRT) return;
        // For Phase 3 minimal GPU test: if shader has u_inputTexture, blit it?
        // We do nothing, just keep currentRT as is
    }

    void Blit(IGpuTexture* src, IRenderTarget* dst) override {
        NullTexture* srcTex = static_cast<NullTexture*>(src);
        NullRenderTarget* dstRT = static_cast<NullRenderTarget*>(dst);
        if (!srcTex || !dstRT || !dstRT->texture) return;
        // Simple CPU copy, with size check
        if (srcTex->data.size() == dstRT->texture->data.size()) {
            dstRT->texture->data = srcTex->data;
        } else {
            // Resize copy: for simplicity, if sizes differ, clear dst and copy as much as possible
            size_t copySize = std::min(srcTex->data.size(), dstRT->texture->data.size());
            memcpy(dstRT->texture->data.data(), srcTex->data.data(), copySize);
        }
    }

    void Present() override {}

    void SetBlendMode(HFRenderBlendMode mode) override { (void)mode; }
    void SetMakeupBlendMode(HFBlendMode mode) override { (void)mode; }
    void SetDepthTest(bool enable) override { (void)enable; }
    void SetCullMode(bool enable) override { (void)enable; }

private:
    bool initialized;
    NullRenderTarget* currentRT;
};

} // namespace huanface
