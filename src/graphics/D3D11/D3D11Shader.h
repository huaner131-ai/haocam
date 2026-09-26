#pragma once

// Runtime HLSL compilation (D3DCompile; d3dcompiler_47 ships with Windows)
// and small constant-buffer helpers. Shaders are embedded into the binary at
// build time from src/graphics/shaders/*.hlsl (see cmake/EmbedShader).

#include <cstdint>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11Buffer;
struct ID3D11SamplerState;

namespace haocam::gfx {

class D3D11ShaderProgram {
public:
    D3D11ShaderProgram() = default;
    ~D3D11ShaderProgram();

    D3D11ShaderProgram(const D3D11ShaderProgram&) = delete;
    D3D11ShaderProgram& operator=(const D3D11ShaderProgram&) = delete;

    // Compiles embedded HLSL sources. Returns false and logs on error.
    bool loadFromEmbeddedSource(ID3D11Device* device, const unsigned char* vsSource,
                                size_t vsSize, const unsigned char* psSource, size_t psSize);

    bool valid() const { return m_pixelShader != nullptr; }
    const std::string& lastError() const { return m_lastError; }

    ID3D11VertexShader* vertexShader() const { return m_vertexShader; }
    ID3D11PixelShader* pixelShader() const { return m_pixelShader; }
    ID3D11Buffer* vertexConstants() const { return m_vsConstants; }
    ID3D11Buffer* pixelConstants() const { return m_psConstants; }

    // Creates/updates the VS constant buffer (size rounded up to 16 bytes).
    bool setVertexConstants(ID3D11DeviceContext* context, const void* data, uint32_t size);
    // Creates/updates the PS constant buffer.
    bool setPixelConstants(ID3D11DeviceContext* context, const void* data, uint32_t size);

private:
    bool updateConstants(ID3D11DeviceContext* context, ID3D11Buffer** buffer, const void* data,
                         uint32_t size);

    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11Buffer* m_vsConstants = nullptr;
    ID3D11Buffer* m_psConstants = nullptr;
    std::string m_lastError;
};

// Shared linear-clamp sampler for all built-in passes.
class D3D11SamplerCache {
public:
    ~D3D11SamplerCache();
    ID3D11SamplerState* linearClamp(ID3D11Device* device);

private:
    ID3D11SamplerState* m_linearClamp = nullptr;
};

} // namespace haocam::gfx
