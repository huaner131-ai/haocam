#include "graphics/D3D11/D3D11Shader.h"

#include <cstring>

#include "core/logging/Logger.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace haocam::gfx {

using Microsoft::WRL::ComPtr;

namespace {
constexpr const char* kCategory = "gpu";

bool compileBlob(const unsigned char* source, size_t size, const char* target,
                 ID3DBlob** outBlob, std::string& error) {
    if (!source || size == 0) {
        error = "shader source missing";
        return false;
    }
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    HRESULT hr = D3DCompile(source, size, nullptr, nullptr, nullptr, "main", target, flags, 0,
                            blob.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr)) {
        error = errors ? static_cast<const char*>(errors->GetBufferPointer())
                       : "D3DCompile failed";
        return false;
    }
    *outBlob = blob.Detach();
    return true;
}
} // namespace

D3D11ShaderProgram::~D3D11ShaderProgram() {
    if (m_vsConstants) m_vsConstants->Release();
    if (m_psConstants) m_psConstants->Release();
    if (m_vertexShader) m_vertexShader->Release();
    if (m_pixelShader) m_pixelShader->Release();
}

bool D3D11ShaderProgram::loadFromEmbeddedSource(ID3D11Device* device,
                                                const unsigned char* vsSource, size_t vsSize,
                                                const unsigned char* psSource, size_t psSize) {
    ComPtr<ID3DBlob> vsBlob, psBlob;
    std::string error;
    if (!compileBlob(vsSource, vsSize, "vs_5_0", vsBlob.GetAddressOf(), error) ||
        !compileBlob(psSource, psSize, "ps_5_0", psBlob.GetAddressOf(), error)) {
        HAOCAM_LOG_ERROR(kCategory, "Shader compile failed: {}", error);
        m_lastError = error;
        return false;
    }
    if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                          nullptr, &m_vertexShader)) ||
        FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                         nullptr, &m_pixelShader))) {
        HAOCAM_LOG_ERROR(kCategory, "CreateVertexShader/CreatePixelShader failed");
        m_lastError = "shader creation failed";
        return false;
    }
    return true;
}

bool D3D11ShaderProgram::updateConstants(ID3D11DeviceContext* context, ID3D11Buffer** buffer,
                                         const void* data, uint32_t size) {
    if (!context || !data || size == 0) return false;

    if (!*buffer) {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = (size + 15) & ~15u;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ID3D11Device* device = nullptr;
        context->GetDevice(&device);
        const bool ok = device && SUCCEEDED(device->CreateBuffer(&desc, nullptr, buffer));
        if (device) device->Release();
        if (!ok) return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(*buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return false;
    std::memcpy(mapped.pData, data, size);
    context->Unmap(*buffer, 0);
    return true;
}

bool D3D11ShaderProgram::setVertexConstants(ID3D11DeviceContext* context, const void* data,
                                            uint32_t size) {
    return updateConstants(context, &m_vsConstants, data, size);
}

bool D3D11ShaderProgram::setPixelConstants(ID3D11DeviceContext* context, const void* data,
                                           uint32_t size) {
    return updateConstants(context, &m_psConstants, data, size);
}

D3D11SamplerCache::~D3D11SamplerCache() {
    if (m_linearClamp) m_linearClamp->Release();
}

ID3D11SamplerState* D3D11SamplerCache::linearClamp(ID3D11Device* device) {
    if (m_linearClamp) return m_linearClamp;
    if (!device) return nullptr;
    D3D11_SAMPLER_DESC desc{};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.MaxAnisotropy = 1;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&desc, &m_linearClamp))) return nullptr;
    return m_linearClamp;
}

} // namespace haocam::gfx
