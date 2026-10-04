/**
 * HuanFace Rendering Abstraction — Phase 3
 * IRenderBackend interface from RENDER_BACKEND.md
 */

#pragma once
#include "../../include/huanface_c_api.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace huanface {

// Rendering blend mode (Phase 3) — distinct from makeup blend mode (Phase 6)
enum class HFRenderBlendMode {
    OPAQUE = 0,
    ALPHA_BLEND = 1,
    ADDITIVE = 2,
    MULTIPLY = 3
};
// Keep old name as alias for backward compatibility in rendering code
using HFBlendModeRender = HFRenderBlendMode;

enum class HFBlendMode {
    Normal = 0,
    Multiply = 1,
    Screen = 2,
    Overlay = 3
};

struct Uniform {
    enum class Type {
        FLOAT,
        VEC2,
        VEC3,
        VEC4,
        COLOR,
        INT,
        TEXTURE
    };
    Type type = Type::FLOAT;
    float floatValue = 0.0f;
    float vec2Value[2] = {0,0};
    float vec3Value[3] = {0,0,0};
    float vec4Value[4] = {0,0,0,0};
    int intValue = 0;
    void* textureValue = nullptr; // IGpuTexture*
};

// Forward declarations
class IGpuTexture;
class IRenderTarget;
class IShader;
class IMesh;

class IGpuTexture {
public:
    virtual ~IGpuTexture() = default;
    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    virtual HFFormat GetFormat() const = 0;
    virtual void* GetNativeHandle() const = 0;
};

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;
    virtual IGpuTexture* GetTexture() = 0;
    virtual void Bind() = 0;
    virtual void Unbind() = 0;
};

class IShader {
public:
    virtual ~IShader() = default;
    virtual void SetUniform(const std::string& name, const Uniform& uniform) = 0;
    virtual void SetTexture(const std::string& name, IGpuTexture* texture) = 0;
};

class IMesh {
public:
    virtual ~IMesh() = default;
    virtual void* GetNativeHandle() const = 0;
    virtual int GetVertexCount() const = 0;
    virtual int GetIndexCount() const = 0;
};

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;
    virtual HFResult Init(void* windowHandle = nullptr) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;
    virtual HFRenderBackendType GetType() const = 0;

    virtual IGpuTexture* CreateTexture(int width, int height, HFFormat format, const void* data) = 0;
    virtual IGpuTexture* CreateTextureFromFile(const std::string& path) = 0;
    virtual void DestroyTexture(IGpuTexture* texture) = 0;

    virtual IRenderTarget* CreateRenderTarget(int width, int height, HFFormat format = HF_FORMAT_RGBA8) = 0;
    virtual void DestroyRenderTarget(IRenderTarget* rt) = 0;
    virtual void SetRenderTarget(IRenderTarget* rt) = 0;

    virtual void Clear(float r, float g, float b, float a) = 0;

    virtual IShader* CreateShader(const std::string& vsSrc, const std::string& fsSrc) = 0;
    virtual IShader* CreateShaderFromFile(const std::string& vsPath, const std::string& fsPath) = 0;
    virtual void DestroyShader(IShader* shader) = 0;

    virtual IMesh* CreateMesh(const std::vector<float>& vertices, const std::vector<int>& indices) = 0;
    virtual void DestroyMesh(IMesh* mesh) = 0;

    virtual void DrawMesh(IMesh* mesh, IShader* shader, const std::map<std::string, Uniform>& uniforms) = 0;
    virtual void Blit(IGpuTexture* src, IRenderTarget* dst) = 0;
    virtual void Present() = 0;

    virtual void SetBlendMode(HFRenderBlendMode mode) = 0;
    // Makeup blend mode setter (Phase 6) — overload for convenience
    virtual void SetMakeupBlendMode(HFBlendMode mode) { (void)mode; }
    virtual void SetDepthTest(bool enable) = 0;
    virtual void SetCullMode(bool enable) = 0;
};

// Factory
std::unique_ptr<IRenderBackend> CreateRenderBackend(HFRenderBackendType type);
std::unique_ptr<IRenderBackend> CreateNullBackend(); // Software null backend for testing

#ifdef _WIN32
std::unique_ptr<IRenderBackend> CreateD3D11Backend();
#endif
std::unique_ptr<IRenderBackend> CreateOpenGLBackend(); // Stub P1

} // namespace huanface
