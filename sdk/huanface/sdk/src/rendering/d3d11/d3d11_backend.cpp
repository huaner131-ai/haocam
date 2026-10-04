/**
 * HuanFace D3D11 Backend Implementation — Phase 8D Production Shader & GPU Timing Verification
 * Real D3D11CreateDevice, feature level, adapter info, ComPtr RAII, resource pooling, timestamp queries, error recovery
 * Production HLSL compile with ID3DInclude handler, no fallback shader, real GPU timing via TIMESTAMP queries
 */

#ifdef _WIN32

#include "d3d11_backend.h"
#include "../../../include/huanface/huanface_image.h"
#include <d3dcompiler.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <filesystem>

#ifdef LoadImage
#undef LoadImage
#endif
#ifdef LoadImageA
#undef LoadImageA
#endif

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace huanface {
namespace fs = std::filesystem;

// ============================================================================
// ID3DInclude handler for production HLSL #include
// ============================================================================
class D3DIncludeHandler : public ID3DInclude {
public:
    std::string baseDir;
    std::vector<std::string> searchPaths;
    // Keep included file data alive
    std::map<std::string, std::vector<char>> fileDataMap;

    D3DIncludeHandler(const std::string& base = "") : baseDir(base) {
        searchPaths.push_back(base);
        searchPaths.push_back("sdk/src/rendering/shaders/");
        searchPaths.push_back("../sdk/src/rendering/shaders/");
        searchPaths.push_back("../../sdk/src/rendering/shaders/");
        searchPaths.push_back("D:/sdk/HuanFace/sdk/src/rendering/shaders/");
        searchPaths.push_back("./");
    }

    HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) override {
        (void)IncludeType;
        (void)pParentData;
        std::string fileName = pFileName;
        std::string foundPath;
        std::vector<char> content;

        // Try baseDir + fileName
        std::vector<std::string> tryPaths;
        if(!baseDir.empty()){
            tryPaths.push_back((fs::path(baseDir) / fileName).string());
        }
        for(auto& sp : searchPaths){
            tryPaths.push_back((fs::path(sp) / fileName).string());
            tryPaths.push_back(sp + fileName);
        }
        tryPaths.push_back(fileName);

        for(auto& tp : tryPaths){
            std::ifstream f(tp, std::ios::binary);
            if(f){
                f.seekg(0, std::ios::end);
                size_t size = (size_t)f.tellg();
                f.seekg(0, std::ios::beg);
                content.resize(size);
                f.read(content.data(), size);
                if((size_t)f.gcount()==size){
                    foundPath = tp;
                    break;
                }
            }
        }

        if(foundPath.empty()){
            std::cerr << "[D3D11Include] Failed to open include: " << fileName << " searched in " << baseDir << std::endl;
            return E_FAIL;
        }

        // Store to keep alive
        fileDataMap[foundPath] = content;
        auto& stored = fileDataMap[foundPath];
        *ppData = stored.data();
        *pBytes = (UINT)stored.size();
        return S_OK;
    }

    HRESULT __stdcall Close(LPCVOID pData) override {
        // Keep data alive until handler destroyed, don't free immediately
        (void)pData;
        return S_OK;
    }
};

D3D11Backend::D3D11Backend() : frameCounter_(0), deviceLost_(false) {}
D3D11Backend::~D3D11Backend() { Shutdown(); }

DXGI_FORMAT D3D11Backend::ToDXGIFormat(HFFormat fmt) {
    switch (fmt) {
        case HF_FORMAT_RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case HF_FORMAT_BGRA8: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case HF_FORMAT_RGB8: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case HF_FORMAT_BGR8: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case HF_FORMAT_R8: return DXGI_FORMAT_R8_UNORM;
        case HF_FORMAT_R32F: return DXGI_FORMAT_R32_FLOAT;
        default: return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

std::string D3D11Backend::FeatureLevelToString(D3D_FEATURE_LEVEL level) {
    switch(level){
        case D3D_FEATURE_LEVEL_11_1: return "11.1";
        case D3D_FEATURE_LEVEL_11_0: return "11.0";
        case D3D_FEATURE_LEVEL_10_1: return "10.1";
        case D3D_FEATURE_LEVEL_10_0: return "10.0";
        case D3D_FEATURE_LEVEL_9_3: return "9.3";
        case D3D_FEATURE_LEVEL_9_2: return "9.2";
        case D3D_FEATURE_LEVEL_9_1: return "9.1";
        default: return "Unknown";
    }
}

void D3D11Backend::QueryAdapterInfo() {
    if(!device) return;
    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if(FAILED(hr)) return;

    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if(FAILED(hr)) return;
    dxgiAdapter_ = adapter;

    DXGI_ADAPTER_DESC desc;
    hr = adapter->GetDesc(&desc);
    if(SUCCEEDED(hr)){
        char descStr[128];
        size_t converted=0;
        wcstombs_s(&converted, descStr, sizeof(descStr), desc.Description, _TRUNCATE);
        adapterInfo_.description = descStr;
        adapterInfo_.dedicatedVideoMemory = desc.DedicatedVideoMemory;
        adapterInfo_.dedicatedSystemMemory = desc.DedicatedSystemMemory;
        adapterInfo_.sharedSystemMemory = desc.SharedSystemMemory;
        switch(desc.VendorId){
            case 0x10DE: adapterInfo_.vendor="NVIDIA"; break;
            case 0x1002: adapterInfo_.vendor="AMD"; break;
            case 0x8086: adapterInfo_.vendor="Intel"; break;
            case 0x1414: adapterInfo_.vendor="Microsoft WARP"; adapterInfo_.isWarp=true; break;
            default: adapterInfo_.vendor="Unknown ("+std::to_string(desc.VendorId)+")"; break;
        }
    }
    adapterInfo_.featureLevel = featureLevel_;
    adapterInfo_.featureLevelStr = FeatureLevelToString(featureLevel_);
}

HFResult D3D11Backend::CreateDeviceAndContext(void* windowHandle) {
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL featureLevel;
    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HRESULT hr;
    if(windowHandle){
        HWND hwnd = (HWND)windowHandle;
        RECT rect;
        GetClientRect(hwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        if(width<=0) width=1280;
        if(height<=0) height=720;
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags, featureLevels, 4, D3D11_SDK_VERSION, &sd, &swapChain, &device, &featureLevel, &context);
        if(FAILED(hr)){
            hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createFlags, featureLevels, 4, D3D11_SDK_VERSION, &sd, &swapChain, &device, &featureLevel, &context);
        }
        if(SUCCEEDED(hr) && swapChain){
            ComPtr<ID3D11Texture2D> backBuffer;
            hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
            if(SUCCEEDED(hr)){
                hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &backBufferRTV);
            }
        }
    } else {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags, featureLevels, 4, D3D11_SDK_VERSION, &device, &featureLevel, &context);
        if(FAILED(hr)){
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createFlags, featureLevels, 4, D3D11_SDK_VERSION, &device, &featureLevel, &context);
        }
    }
    if(FAILED(hr) || !device || !context){
        return HF_RESULT_FAIL;
    }
    featureLevel_ = featureLevel;
    QueryAdapterInfo();
    resourcePool_.Init(this);
    return HF_RESULT_OK;
}

HFResult D3D11Backend::Init(void* windowHandle) {
    if (initialized) return HF_RESULT_OK;
    HFResult res = CreateDeviceAndContext(windowHandle);
    if (res != HF_RESULT_OK) return res;
    // Phase 9 FIX: Create default rasterizer state with CULL_NONE to ensure fullscreen quad draws regardless of winding
    // Previous default (CULL_BACK, FrontCCW FALSE) culled CCW quad {0,1,2,0,2,3} -> black output MAE 101
    if (device && context) {
        D3D11_RASTERIZER_DESC rsDesc = {};
        rsDesc.FillMode = D3D11_FILL_SOLID;
        rsDesc.CullMode = D3D11_CULL_NONE;
        rsDesc.FrontCounterClockwise = FALSE;
        rsDesc.DepthClipEnable = TRUE;
        rsDesc.ScissorEnable = FALSE;
        ComPtr<ID3D11RasterizerState> rsState;
        HRESULT hr = device->CreateRasterizerState(&rsDesc, &rsState);
        if (SUCCEEDED(hr) && rsState) {
            context->RSSetState(rsState.Get());
            std::cout << "[D3D11] Default rasterizer CULL_NONE set in Init" << std::endl;
        }
        // Also set default blend disabled (opaque) and depth disabled
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = FALSE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        ComPtr<ID3D11BlendState> blendState;
        hr = device->CreateBlendState(&blendDesc, &blendState);
        if (SUCCEEDED(hr) && blendState) {
            float bf[4]={0,0,0,0};
            context->OMSetBlendState(blendState.Get(), bf, 0xFFFFFFFF);
        }
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = FALSE;
        dsDesc.StencilEnable = FALSE;
        ComPtr<ID3D11DepthStencilState> dsState;
        hr = device->CreateDepthStencilState(&dsDesc, &dsState);
        if (SUCCEEDED(hr) && dsState) {
            context->OMSetDepthStencilState(dsState.Get(), 0);
        }
    }
    initialized = true;
    return HF_RESULT_OK;
}

void D3D11Backend::Shutdown() {
    if (!initialized) return;
    if (context) context->ClearState();
    backBufferRTV.Reset();
    swapChain.Reset();
    context.Reset();
    device.Reset();
    initialized = false;
    currentRT = nullptr;
}

IGpuTexture* D3D11Backend::CreateTexture(int width, int height, HFFormat format, const void* data) {
    if (!initialized || !device) return nullptr;
    if (width <=0 || height <=0) return nullptr;
    DXGI_FORMAT dxgiFmt = ToDXGIFormat(format);
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = dxgiFmt;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initData = {};
    D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
    if (data) {
        int bpp = 4;
        if (format == HF_FORMAT_R8) bpp = 1;
        else if (format == HF_FORMAT_R32F) bpp = 4;
        initData.pSysMem = data;
        initData.SysMemPitch = width * bpp;
        pInitData = &initData;
    }
    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&desc, pInitData, &tex);
    if (FAILED(hr) || !tex) return nullptr;
    ComPtr<ID3D11ShaderResourceView> srv;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = dxgiFmt;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(tex.Get(), &srvDesc, &srv);
    if(FAILED(hr) || !srv){
        std::cerr << "[D3D11] CreateShaderResourceView failed for format=" << (int)format << " dxgiFmt=" << (int)dxgiFmt << " w=" << width << " h=" << height << " hr=" << std::hex << hr << std::endl;
    } else {
        std::cout << "[D3D11] CreateTexture w=" << width << " h=" << height << " format=" << (int)format << " dxgiFmt=" << (int)dxgiFmt << " srv=" << (srv? "valid":"NULL") << " hr=" << std::hex << hr << std::endl;
    }
    return new D3D11Texture(width, height, format, tex, srv);
}

IGpuTexture* D3D11Backend::CreateTextureFromFile(const std::string& path) {
    if (path.empty()) return nullptr;
    if (!initialized || !device) return nullptr;
    HFImage img;
    std::string err;
    if (!ImageLoader::LoadImage(path, img, err)) {
        std::cerr << "[D3D11] CreateTextureFromFile failed to load " << path << " err=" << err << std::endl;
        return nullptr;
    }
    return CreateTexture(img.width, img.height, HF_FORMAT_RGBA8, img.data.data());
}

IGpuTexture* D3D11Backend::CreateTextureFromMemory(const uint8_t* pngData, size_t pngSize) {
    if (!pngData || pngSize==0) return nullptr;
    if (!initialized || !device) return nullptr;
    HFImage img;
    std::string err;
    if (!ImageLoader::LoadImageFromMemory(pngData, pngSize, img, err)) {
        std::cerr << "[D3D11] CreateTextureFromMemory failed err=" << err << std::endl;
        return nullptr;
    }
    return CreateTexture(img.width, img.height, HF_FORMAT_RGBA8, img.data.data());
}

void D3D11Backend::DestroyTexture(IGpuTexture* texture) {
    delete static_cast<D3D11Texture*>(texture);
}

IRenderTarget* D3D11Backend::CreateRenderTarget(int width, int height, HFFormat format) {
    if (!initialized || !device) return nullptr;
    if (width <=0 || height <=0) return nullptr;
    DXGI_FORMAT dxgiFmt = ToDXGIFormat(format);
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = dxgiFmt;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &tex);
    if (FAILED(hr) || !tex) return nullptr;
    ComPtr<ID3D11RenderTargetView> rtv;
    hr = device->CreateRenderTargetView(tex.Get(), nullptr, &rtv);
    if (FAILED(hr) || !rtv) return nullptr;
    ComPtr<ID3D11ShaderResourceView> srv;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = dxgiFmt;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(tex.Get(), &srvDesc, &srv);
    return new D3D11RenderTarget(width, height, format, tex, rtv, srv);
}

void D3D11Backend::DestroyRenderTarget(IRenderTarget* rt) {
    delete static_cast<D3D11RenderTarget*>(rt);
}

void D3D11Backend::SetRenderTarget(IRenderTarget* rt) {
    if (!initialized || !context) return;
    currentRT = rt;
    if (!rt) {
        if (backBufferRTV) {
            context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), nullptr);
        } else {
            ID3D11RenderTargetView* nullRTV = nullptr;
            context->OMSetRenderTargets(1, &nullRTV, nullptr);
        }
        return;
    }
    D3D11RenderTarget* d3dRT = static_cast<D3D11RenderTarget*>(rt);
    if (d3dRT && d3dRT->rtv) {
        // Phase 9 FIX: Ensure RTV is set and viewport matches RT dimensions, and rasterizer CULL_NONE for fullscreen quad
        // Previous code did not set rasterizer, default cull back culled CCW quad -> black output
        context->OMSetRenderTargets(1, d3dRT->rtv.GetAddressOf(), nullptr);
        D3D11_VIEWPORT vp = {};
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        vp.Width = (FLOAT)d3dRT->width;
        vp.Height = (FLOAT)d3dRT->height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);
        // Ensure rasterizer CULL_NONE
        D3D11_RASTERIZER_DESC rsDesc = {};
        rsDesc.FillMode = D3D11_FILL_SOLID;
        rsDesc.CullMode = D3D11_CULL_NONE;
        rsDesc.FrontCounterClockwise = FALSE;
        rsDesc.DepthClipEnable = TRUE;
        ComPtr<ID3D11RasterizerState> rsState;
        HRESULT hr = device->CreateRasterizerState(&rsDesc, &rsState);
        if (SUCCEEDED(hr) && rsState) {
            context->RSSetState(rsState.Get());
        }
        std::cout << "[D3D11] SetRenderTarget RT=" << d3dRT->width << "x" << d3dRT->height << " RTV=" << (d3dRT->rtv?"valid":"NULL") << " Viewport=" << vp.Width << "x" << vp.Height << " CULL_NONE set" << std::endl;
    } else {
        std::cerr << "[D3D11] SetRenderTarget WARNING: d3dRT null or rtv null!" << std::endl;
    }
}

void D3D11Backend::Clear(float r, float g, float b, float a) {
    if (!initialized || !context) return;
    float color[4] = {r,g,b,a};
    if (currentRT) {
        D3D11RenderTarget* d3dRT = static_cast<D3D11RenderTarget*>(currentRT);
        if (d3dRT && d3dRT->rtv) {
            context->ClearRenderTargetView(d3dRT->rtv.Get(), color);
        }
    } else if (backBufferRTV) {
        context->ClearRenderTargetView(backBufferRTV.Get(), color);
    }
}

// Production shader compile with include handler, no fallback
IShader* D3D11Backend::CreateShader(const std::string& vsSrc, const std::string& fsSrc) {
    if (!initialized || !device) return nullptr;
    D3D11Shader* shader = new D3D11Shader();

    std::string vertexSource = vsSrc;
    std::string pixelSource = fsSrc;

    if (vertexSource.empty()) {
        vertexSource = "struct VS_INPUT { float4 pos : POSITION; float2 uv : TEXCOORD0; }; struct PS_INPUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; PS_INPUT main(VS_INPUT input) { PS_INPUT output; output.pos = input.pos; output.uv = input.uv; return output; }";
    }
    if (pixelSource.empty()) {
        pixelSource = "Texture2D inputTexture : register(t0); SamplerState samLinear : register(s0); float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET { return inputTexture.Sample(samLinear, uv); }";
    }

    ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    HRESULT hr;

    // Try production entry points for VS: VSMain, main
    std::vector<std::string> vsEntries = {"VSMain", "main", "VS_INPUT"};
    bool vsCompiled=false;
    D3DIncludeHandler vsInclude("");
    for(auto& entry : vsEntries){
        errorBlob.Reset();
        vsBlob.Reset();
        hr = D3DCompile(vertexSource.c_str(), vertexSource.size(), "vs.hlsl", nullptr, &vsInclude, entry.c_str(), "vs_5_0", 0, 0, &vsBlob, &errorBlob);
        if(SUCCEEDED(hr) && vsBlob){
            vsCompiled=true;
            break;
        }
    }
    if(!vsCompiled){
        if (errorBlob) {
            std::string errMsg((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
            // Only log if not default VS (avoid spam)
            if(vertexSource.find("VS_INPUT") == std::string::npos || vertexSource.size()>500){
                std::cerr << "[D3D11] VS compile error: " << errMsg << std::endl;
            }
        }
        // No fallback — return shader with null vs, but still try PS
    } else {
        hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &shader->vs);
        if(FAILED(hr)){
            std::cerr << "[D3D11] CreateVertexShader failed hr=" << std::hex << hr << std::endl;
        } else {
            // Create input layout for fullscreen quad: POSITION float3 + TEXCOORD float2 = 5 floats stride 20
            D3D11_INPUT_ELEMENT_DESC layout[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}
            };
            hr = device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &shader->inputLayout);
            if(FAILED(hr)){
                std::cerr << "[D3D11] CreateInputLayout failed hr=" << std::hex << hr << " -- draw will produce black (no IA)" << std::endl;
            }
        }
    }

    // Try production entry points for PS: PSSmoothing, PSTextureRefinement, PSBlemishReduction, PSToneAdjustment, PSBrightness, PSContrast, PSBeautyFinal, PSFoundation, PSLip, PSBlush, PSEyeshadow, PSBlend, PSNormal, PSMultiply, PSScreen, PSOverlay, main
    std::vector<std::string> psEntries = {
        "PSSmoothing", "PSSmoothingGaussian", "PSTextureRefinement", "PSBlemishReduction", "PSToneAdjustment",
        "PSBrightness", "PSContrast", "PSBrightnessContrast", "PSBeautyFinal",
        "PSFoundation", "PSLip", "PSUpperLip", "PSLowerLip", "PSBlush", "PSLeftCheek", "PSRightCheek",
        "PSEyeshadow", "PSEyeliner", "PSEyebrow", "PSEyelash", "PSPupil", "PSLeftEye", "PSRightEye",
        "PSBlend", "PSNormal", "PSMultiply", "PSScreen", "PSOverlay",
        "main"
    };
    bool psCompiled=false;
    D3DIncludeHandler psInclude("");
    std::string psLastError;
    for(auto& entry : psEntries){
        errorBlob.Reset();
        psBlob.Reset();
        hr = D3DCompile(pixelSource.c_str(), pixelSource.size(), "ps.hlsl", nullptr, &psInclude, entry.c_str(), "ps_5_0", 0, 0, &psBlob, &errorBlob);
        if(SUCCEEDED(hr) && psBlob){
            psCompiled=true;
            // Avoid spam for default PS
            if(pixelSource.find("inputTexture") == std::string::npos || pixelSource.size()>500){
                std::cout << "[D3D11] PS compiled with entry point: " << entry << std::endl;
            }
            break;
        } else {
            if(errorBlob){
                psLastError.assign((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
            }
        }
    }
    if(!psCompiled){
        if (errorBlob) {
            std::string errMsg((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
            std::cerr << "[D3D11] PS compile error (all entry points failed): " << errMsg << std::endl;
        }
        // No fallback shader — return nullptr to indicate FAIL per GATE 1
        delete shader;
        return nullptr;
    } else {
        hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &shader->ps);
        if(FAILED(hr)){
            std::cerr << "[D3D11] CreatePixelShader failed hr=" << std::hex << hr << std::endl;
            delete shader;
            return nullptr;
        }
    }

    return shader;
}

IShader* D3D11Backend::CreateShaderFromFile(const std::string& vsPath, const std::string& fsPath) {
    std::string vsSrc, psSrc;
    std::string vsBase, psBase;
    if (!vsPath.empty()) {
        std::ifstream vsFile(vsPath);
        if (vsFile) {
            vsSrc.assign((std::istreambuf_iterator<char>(vsFile)), std::istreambuf_iterator<char>());
            vsBase = fs::path(vsPath).parent_path().string();
        } else {
            std::cerr << "[D3D11] Failed to open VS file: " << vsPath << std::endl;
            return nullptr;
        }
    }
    if (!fsPath.empty()) {
        std::ifstream fsFile(fsPath);
        if (fsFile) {
            psSrc.assign((std::istreambuf_iterator<char>(fsFile)), std::istreambuf_iterator<char>());
            psBase = fs::path(fsPath).parent_path().string();
        } else {
            std::cerr << "[D3D11] Failed to open PS file: " << fsPath << std::endl;
            return nullptr;
        }
    }
    if (vsSrc.empty() && psSrc.empty()) {
        std::cerr << "[D3D11] Both VS and PS sources empty" << std::endl;
        return nullptr;
    }

    if (!initialized || !device) return nullptr;
    D3D11Shader* shader = new D3D11Shader();
    ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    HRESULT hr;

    // VS compile with include handler using vsBase if available, else fs base or empty
    std::string vsEffectiveSrc = vsSrc;
    if (vsEffectiveSrc.empty()) {
        vsEffectiveSrc = "struct VS_INPUT { float4 pos : POSITION; float2 uv : TEXCOORD0; }; struct PS_INPUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; PS_INPUT main(VS_INPUT input) { PS_INPUT output; output.pos = input.pos; output.uv = input.uv; return output; }";
    }
    std::vector<std::string> vsEntries = {"VSMain", "main"};
    bool vsCompiled=false;
    std::string vsIncludeBase = !vsBase.empty() ? vsBase : psBase;
    D3DIncludeHandler vsInclude(vsIncludeBase);
    std::string vsLastError;
    for(auto& entry : vsEntries){
        errorBlob.Reset(); vsBlob.Reset();
        hr = D3DCompile(vsEffectiveSrc.c_str(), vsEffectiveSrc.size(), vsPath.empty()? "vs.hlsl" : vsPath.c_str(), nullptr, &vsInclude, entry.c_str(), "vs_5_0", 0, 0, &vsBlob, &errorBlob);
        if(SUCCEEDED(hr) && vsBlob){ 
            vsCompiled=true; 
            if(!vsPath.empty() && vsPath!="vs.hlsl"){
                std::cout << "[D3D11] VS file compiled with entry point: " << entry << " file: " << vsPath << std::endl;
            }
            break; 
        } else {
            if(errorBlob){
                vsLastError.assign((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
                // Only log if file path is production file, not default vs.hlsl spam
                if(!vsPath.empty() && vsPath!="vs.hlsl"){
                    std::cerr << "[D3D11] VS compile failed entry=" << entry << " file=" << vsPath << " : " << vsLastError << std::endl;
                }
            } else {
                if(!vsPath.empty() && vsPath!="vs.hlsl"){
                    std::cerr << "[D3D11] VS compile failed entry=" << entry << " file=" << vsPath << " no error blob hr=" << std::hex << hr << std::endl;
                }
            }
        }
    }
    if(!vsCompiled && !vsPath.empty()){
        if(!vsLastError.empty()){
            std::cerr << "[D3D11] VS file compile error (all entries failed) file=" << vsPath << " : " << vsLastError << std::endl;
        }
    }
    if(vsCompiled){
        hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &shader->vs);
        if(FAILED(hr)){
            std::cerr << "[D3D11] CreateVertexShader failed hr=" << std::hex << hr << std::endl;
        } else {
            D3D11_INPUT_ELEMENT_DESC layout[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}
            };
            hr = device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &shader->inputLayout);
            if(FAILED(hr)){
                std::cerr << "[D3D11] CreateInputLayout (file) failed hr=" << std::hex << hr << std::endl;
            }
        }
    }

    // PS compile with include handler using psBase
    std::string psEffectiveSrc = psSrc;
    if (psEffectiveSrc.empty()) {
        psEffectiveSrc = "Texture2D inputTexture : register(t0); SamplerState samLinear : register(s0); float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET { return inputTexture.Sample(samLinear, uv); }";
    }
    std::vector<std::string> psEntries = {
        "PSSmoothing", "PSSmoothingGaussian", "PSTextureRefinement", "PSBlemishReduction", "PSToneAdjustment",
        "PSBrightness", "PSContrast", "PSBrightnessContrast", "PSBeautyFinal",
        "PSFoundation", "PSLip", "PSUpperLip", "PSLowerLip", "PSBlush", "PSLeftCheek", "PSRightCheek",
        "PSEyeshadow", "PSEyeliner", "PSEyebrow", "PSEyelash", "PSPupil", "PSLeftEye", "PSRightEye",
        "PSBlend", "PSNormal", "PSMultiply", "PSScreen", "PSOverlay",
        "main"
    };
    bool psCompiled=false;
    D3DIncludeHandler psInclude(psBase.empty()? vsBase : psBase);
    std::string lastError;
    for(auto& entry : psEntries){
        errorBlob.Reset(); psBlob.Reset();
        hr = D3DCompile(psEffectiveSrc.c_str(), psEffectiveSrc.size(), fsPath.empty()? "ps.hlsl" : fsPath.c_str(), nullptr, &psInclude, entry.c_str(), "ps_5_0", 0, 0, &psBlob, &errorBlob);
        if(SUCCEEDED(hr) && psBlob){
            psCompiled=true;
            if(!fsPath.empty() && fsPath!="ps.hlsl"){
                std::cout << "[D3D11] PS file compiled with entry point: " << entry << " file: " << fsPath << std::endl;
            }
            break;
        } else {
            if(errorBlob){
                std::string errMsg((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
                lastError = errMsg;
                // X3501 entrypoint not found is expected when trying multiple production entry points — only log if not X3501 or if last entry main
                bool isEntryNotFound = errMsg.find("X3501") != std::string::npos;
                if(!fsPath.empty() && fsPath!="ps.hlsl" && (!isEntryNotFound || entry=="main")){
                    std::cerr << "[D3D11] PS compile failed entry=" << entry << " file=" << fsPath << " : " << errMsg << std::endl;
                }
            } else {
                if(!fsPath.empty() && fsPath!="ps.hlsl"){
                    std::cerr << "[D3D11] PS compile failed entry=" << entry << " file=" << fsPath << " no error blob hr=" << std::hex << hr << std::endl;
                }
            }
        }
    }
    if(!psCompiled){
        if(!lastError.empty()){
            std::cerr << "[D3D11] PS file compile error (all entry points failed) file=" << fsPath << " : " << lastError << std::endl;
        }
        delete shader;
        return nullptr;
    } else {
        hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &shader->ps);
        if(FAILED(hr)){
            std::cerr << "[D3D11] CreatePixelShader failed hr=" << std::hex << hr << std::endl;
            delete shader;
            return nullptr;
        }
    }
    return shader;
}

IShader* D3D11Backend::CreateShaderFromFileWithEntry(const std::string& vsPath, const std::string& fsPath, const std::string& psEntry) {
    std::string vsSrc, psSrc;
    std::string vsBase, psBase;
    if (!vsPath.empty()) {
        std::ifstream vsFile(vsPath);
        if (vsFile) {
            vsSrc.assign((std::istreambuf_iterator<char>(vsFile)), std::istreambuf_iterator<char>());
            vsBase = fs::path(vsPath).parent_path().string();
        } else {
            std::cerr << "[D3D11] Failed to open VS file: " << vsPath << std::endl;
            return nullptr;
        }
    }
    if (!fsPath.empty()) {
        std::ifstream fsFile(fsPath);
        if (fsFile) {
            psSrc.assign((std::istreambuf_iterator<char>(fsFile)), std::istreambuf_iterator<char>());
            psBase = fs::path(fsPath).parent_path().string();
        } else {
            std::cerr << "[D3D11] Failed to open PS file: " << fsPath << std::endl;
            return nullptr;
        }
    }
    if (vsSrc.empty() && psSrc.empty()) return nullptr;
    if (!initialized || !device) return nullptr;
    D3D11Shader* shader = new D3D11Shader();
    ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    HRESULT hr;

    std::string vsEffectiveSrc = vsSrc;
    if (vsEffectiveSrc.empty()) {
        vsEffectiveSrc = "struct VS_INPUT { float4 pos : POSITION; float2 uv : TEXCOORD0; }; struct PS_INPUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; PS_INPUT main(VS_INPUT input) { PS_INPUT output; output.pos = input.pos; output.uv = input.uv; return output; }";
    }
    std::vector<std::string> vsEntries = {"VSMain", "main"};
    bool vsCompiled=false;
    std::string vsIncludeBase = !vsBase.empty() ? vsBase : psBase;
    D3DIncludeHandler vsInclude(vsIncludeBase);
    for(auto& entry : vsEntries){
        errorBlob.Reset(); vsBlob.Reset();
        hr = D3DCompile(vsEffectiveSrc.c_str(), vsEffectiveSrc.size(), vsPath.empty()? "vs.hlsl" : vsPath.c_str(), nullptr, &vsInclude, entry.c_str(), "vs_5_0", 0, 0, &vsBlob, &errorBlob);
        if(SUCCEEDED(hr) && vsBlob){ vsCompiled=true; break; }
    }
    if(vsCompiled){
        hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &shader->vs);
        if(SUCCEEDED(hr)){
            D3D11_INPUT_ELEMENT_DESC layout[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}
            };
            device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &shader->inputLayout);
        }
    }

    std::string psEffectiveSrc = psSrc;
    if (psEffectiveSrc.empty()) {
        psEffectiveSrc = "Texture2D inputTexture : register(t0); SamplerState samLinear : register(s0); float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET { return inputTexture.Sample(samLinear, uv); }";
    }
    D3DIncludeHandler psInclude(psBase.empty()? vsBase : psBase);
    errorBlob.Reset(); psBlob.Reset();
    hr = D3DCompile(psEffectiveSrc.c_str(), psEffectiveSrc.size(), fsPath.empty()? "ps.hlsl" : fsPath.c_str(), nullptr, &psInclude, psEntry.c_str(), "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if(SUCCEEDED(hr) && psBlob){
        hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &shader->ps);
        if(SUCCEEDED(hr)){
            std::cout << "[D3D11] PS file compiled with specific entry: " << psEntry << " file: " << fsPath << std::endl;
            return shader;
        } else {
            std::cerr << "[D3D11] CreatePixelShader failed for entry " << psEntry << " hr=" << std::hex << hr << std::endl;
            delete shader;
            return nullptr;
        }
    } else {
        std::string errMsg;
        if(errorBlob) errMsg.assign((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
        std::cerr << "[D3D11] PS compile failed for specific entry=" << psEntry << " file=" << fsPath << " : " << errMsg << std::endl;
        delete shader;
        return nullptr;
    }
}

void D3D11Backend::DestroyShader(IShader* shader) {
    delete static_cast<D3D11Shader*>(shader);
}

IMesh* D3D11Backend::CreateMesh(const std::vector<float>& vertices, const std::vector<int>& indices) {
    if (!initialized || !device) return nullptr;
    D3D11Mesh* mesh = new D3D11Mesh(vertices, indices);
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = (UINT)(vertices.size() * sizeof(float));
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();
    device->CreateBuffer(&vbDesc, vertices.empty() ? nullptr : &vbData, &mesh->vertexBuffer);
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = (UINT)(indices.size() * sizeof(int));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();
    device->CreateBuffer(&ibDesc, indices.empty() ? nullptr : &ibData, &mesh->indexBuffer);
    return mesh;
}

void D3D11Backend::DestroyMesh(IMesh* mesh) {
    delete static_cast<D3D11Mesh*>(mesh);
}

void D3D11Backend::DrawMesh(IMesh* mesh, IShader* shader, const std::map<std::string, Uniform>& uniforms) {
    if (!initialized || !context || !mesh || !shader) {
        std::cerr << "[D3D11] DrawMesh early out: init=" << initialized << " ctx=" << (context?"valid":"NULL") << " mesh=" << (mesh?"valid":"NULL") << " shader=" << (shader?"valid":"NULL") << std::endl;
        return;
    }
    (void)uniforms;
    D3D11Mesh* d3dMesh = static_cast<D3D11Mesh*>(mesh);
    D3D11Shader* d3dShader = static_cast<D3D11Shader*>(shader);
    if (!d3dShader->vs) {
        std::cerr << "[D3D11] DrawMesh WARNING: VS NULL!" << std::endl;
    }
    if (!d3dShader->ps) {
        std::cerr << "[D3D11] DrawMesh WARNING: PS NULL!" << std::endl;
    }
    if (!d3dShader->inputLayout) {
        std::cerr << "[D3D11] DrawMesh WARNING: InputLayout NULL! Draw will produce black (no IA)" << std::endl;
    }
    if (d3dShader->vs) context->VSSetShader(d3dShader->vs.Get(), nullptr, 0);
    if (d3dShader->ps) context->PSSetShader(d3dShader->ps.Get(), nullptr, 0);
    if (d3dShader->inputLayout) context->IASetInputLayout(d3dShader->inputLayout.Get());

    // Phase 9 FIX: Ensure rasterizer CULL_NONE for fullscreen quad, and viewport is set
    // Previously cull back culled CCW quad -> black
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> rsState;
    HRESULT hr = device->CreateRasterizerState(&rsDesc, &rsState);
    if (SUCCEEDED(hr) && rsState) {
        context->RSSetState(rsState.Get());
    }

    if (d3dMesh->vertexBuffer) {
        UINT stride = 6 * sizeof(float); // POSITION float4 + TEXCOORD float2 = 6 floats = 24 bytes
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, d3dMesh->vertexBuffer.GetAddressOf(), &stride, &offset);
        std::cout << "[D3D11] DrawMesh VB stride=" << stride << " vertexCount=" << (d3dMesh->vertices.size()/6) << " inputLayout=" << (d3dShader->inputLayout?"valid":"NULL") << std::endl;
    } else {
        std::cerr << "[D3D11] DrawMesh WARNING: VertexBuffer NULL!" << std::endl;
    }
    if (d3dMesh->indexBuffer) {
        context->IASetIndexBuffer(d3dMesh->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        std::cout << "[D3D11] DrawMesh IB indexCount=" << d3dMesh->indices.size() << std::endl;
    } else {
        std::cerr << "[D3D11] DrawMesh WARNING: IndexBuffer NULL!" << std::endl;
    }
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if (d3dMesh->indexBuffer && !d3dMesh->indices.empty()) {
        context->DrawIndexed((UINT)d3dMesh->indices.size(), 0, 0);
        std::cout << "[D3D11] DrawIndexed called count=" << d3dMesh->indices.size() << std::endl;
    } else if (!d3dMesh->vertices.empty()) {
        context->Draw((UINT)(d3dMesh->vertices.size() / 6), 0);
        std::cout << "[D3D11] Draw called vertexCount=" << (d3dMesh->vertices.size()/6) << std::endl;
    } else {
        std::cerr << "[D3D11] DrawMesh WARNING: No vertices to draw!" << std::endl;
    }
}

void D3D11Backend::Blit(IGpuTexture* src, IRenderTarget* dst) {
    if (!initialized || !context || !src || !dst) return;
    D3D11Texture* srcTex = static_cast<D3D11Texture*>(src);
    D3D11RenderTarget* dstRT = static_cast<D3D11RenderTarget*>(dst);
    if (!srcTex || !srcTex->texture || !dstRT || !dstRT->texture) return;
    context->CopyResource(dstRT->texture.Get(), srcTex->texture.Get());
}

void D3D11Backend::Present() {
    if (swapChain) {
        swapChain->Present(1, 0);
    }
}

void D3D11Backend::SetBlendMode(HFRenderBlendMode mode) {
    if(!initialized || !device || !context) return;
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if(mode==HFRenderBlendMode::ADDITIVE){
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    } else if(mode==HFRenderBlendMode::MULTIPLY){
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    }
    ComPtr<ID3D11BlendState> blendState;
    HRESULT hr = device->CreateBlendState(&blendDesc, &blendState);
    if(SUCCEEDED(hr) && blendState){
        float blendFactor[4]={0,0,0,0};
        context->OMSetBlendState(blendState.Get(), blendFactor, 0xffffffff);
    }
}

void D3D11Backend::SetMakeupBlendMode(HFBlendMode mode) {
    if(!initialized || !device || !context) return;
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    ComPtr<ID3D11BlendState> blendState;
    HRESULT hr = device->CreateBlendState(&blendDesc, &blendState);
    if(SUCCEEDED(hr) && blendState){
        float blendFactor[4]={0,0,0,0};
        context->OMSetBlendState(blendState.Get(), blendFactor, 0xffffffff);
    }
}

void D3D11Backend::SetDepthTest(bool enable) {
    (void)enable;
}

void D3D11Backend::SetCullMode(bool enable) {
    (void)enable;
    if(!initialized || !device || !context) return;
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = enable ? D3D11_CULL_BACK : D3D11_CULL_NONE;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> rsState;
    HRESULT hr = device->CreateRasterizerState(&rsDesc, &rsState);
    if(SUCCEEDED(hr) && rsState){
        context->RSSetState(rsState.Get());
    }
}

HFResult D3D11Backend::CreateTimestampQueries(GPUTimestampQuery& query) {
    if(!device) return HF_RESULT_FAIL;
    D3D11_QUERY_DESC desc = {};
    desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    HRESULT hr = device->CreateQuery(&desc, &query.disjointQuery);
    if(FAILED(hr)) return HF_RESULT_FAIL;
    desc.Query = D3D11_QUERY_TIMESTAMP;
    hr = device->CreateQuery(&desc, &query.startQuery);
    if(FAILED(hr)) return HF_RESULT_FAIL;
    hr = device->CreateQuery(&desc, &query.endQuery);
    if(FAILED(hr)) return HF_RESULT_FAIL;
    query.isStarted=false;
    query.gpuMs=0;
    return HF_RESULT_OK;
}

bool D3D11Backend::BeginGPUTimestamp(GPUTimestampQuery& query) {
    if(!context || !query.disjointQuery || !query.startQuery) return false;
    context->Begin(query.disjointQuery.Get());
    context->End(query.startQuery.Get());
    query.isStarted=true;
    return true;
}

bool D3D11Backend::EndGPUTimestamp(GPUTimestampQuery& query) {
    if(!context || !query.disjointQuery || !query.endQuery || !query.isStarted) return false;
    context->End(query.endQuery.Get());
    context->End(query.disjointQuery.Get());
    query.isStarted=false;
    return true;
}

double D3D11Backend::GetGPUTimestampMs(GPUTimestampQuery& query) {
    if(!context || !query.disjointQuery || !query.startQuery || !query.endQuery) return 0;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
    HRESULT hr;
    // Wait with timeout handling
    int attempts=0;
    while((hr=context->GetData(query.disjointQuery.Get(), &disjointData, sizeof(disjointData), 0))==S_FALSE){
        if(attempts++>1000000) return 0; // timeout
    }
    if(FAILED(hr)) return 0;
    if(disjointData.Disjoint){
        std::cerr << "[D3D11] GPU timestamp disjoint TRUE — measurement invalid" << std::endl;
        return 0;
    }
    if(disjointData.Frequency==0){
        std::cerr << "[D3D11] GPU timestamp frequency 0 — invalid" << std::endl;
        return 0;
    }
    UINT64 startTime=0, endTime=0;
    attempts=0;
    while((hr=context->GetData(query.startQuery.Get(), &startTime, sizeof(startTime), 0))==S_FALSE){
        if(attempts++>1000000) return 0;
    }
    if(FAILED(hr)) return 0;
    attempts=0;
    while((hr=context->GetData(query.endQuery.Get(), &endTime, sizeof(endTime), 0))==S_FALSE){
        if(attempts++>1000000) return 0;
    }
    if(FAILED(hr)) return 0;
    if(endTime < startTime){
        std::cerr << "[D3D11] GPU timestamp ordering invalid: end < start" << std::endl;
        return 0;
    }
    UINT64 delta = endTime - startTime;
    double ms = (double)delta / (double)disjointData.Frequency * 1000.0;
    query.gpuMs=ms;
    return ms;
}

IRenderTarget* D3D11Backend::AcquirePooledRenderTarget(int width, int height, HFFormat format) {
    return resourcePool_.AcquireRenderTarget(width,height,format);
}
void D3D11Backend::ReleasePooledRenderTarget(IRenderTarget* rt) {
    resourcePool_.ReleaseRenderTarget(rt);
}
IGpuTexture* D3D11Backend::AcquirePooledTexture(int width, int height, HFFormat format) {
    return resourcePool_.AcquireTexture(width,height,format,0);
}
void D3D11Backend::ReleasePooledTexture(IGpuTexture* tex) {
    resourcePool_.ReleaseTexture(tex);
}
void D3D11Backend::OnResolutionChanged() {
    resourcePool_.OnResolutionChanged();
}
void D3D11Backend::BeginFrame() {
    frameCounter_++;
    resourcePool_.BeginFrame();
}
void D3D11Backend::EndFrame() {
    resourcePool_.EndFrame();
    if(context) context->Flush();
}

bool D3D11Backend::IsDeviceLost() {
    if(!device) return true;
    HRESULT hr = device->GetDeviceRemovedReason();
    return FAILED(hr);
}

HFResult D3D11Backend::HandleDeviceLost() {
    if(!IsDeviceLost()) return HF_RESULT_OK;
    deviceLost_=true;
    resourcePool_.Clear();
    backBufferRTV.Reset();
    swapChain.Reset();
    context.Reset();
    device.Reset();
    initialized=false;
    HFResult res = CreateDeviceAndContext(nullptr);
    if(res==HF_RESULT_OK){
        initialized=true;
        deviceLost_=false;
        return HF_RESULT_OK;
    }
    return HF_RESULT_FAIL;
}

std::unique_ptr<IRenderBackend> CreateD3D11Backend() {
    return std::make_unique<D3D11Backend>();
}

} // namespace huanface

#endif // _WIN32
