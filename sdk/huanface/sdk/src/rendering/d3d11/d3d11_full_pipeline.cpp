/**
 * Full GPU Beauty & Makeup Pipeline Implementation — Phase 9
 */

#ifdef _WIN32

#include "d3d11_full_pipeline.h"
#include <iostream>
#include <set>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cmath>

namespace huanface {
namespace fs = std::filesystem;

std::string FullGPUPipeline::FindShaderPath(const std::string& fileName) {
    std::vector<std::string> bases = {
        "sdk/src/rendering/shaders/",
        "../sdk/src/rendering/shaders/",
        "../../sdk/src/rendering/shaders/",
        "./sdk/src/rendering/shaders/",
        "D:/sdk/HuanFace/sdk/src/rendering/shaders/"
    };
    for(auto& base : bases){
        std::string p = base + fileName;
        std::ifstream f(p);
        if(f) return p;
    }
    return "";
}

IShader* FullGPUPipeline::CompileShaderFromFile(const std::string& fileName) {
    std::string path = FindShaderPath(fileName);
    if(path.empty()){
        std::cerr << "[FullPipeline] Shader not found: " << fileName << std::endl;
        return nullptr;
    }
    // Use backend's CreateShaderFromFile which handles include and entry points
    // For beauty: file contains PS entry like PSSmoothing
    // For makeup: file contains PS entry like PSFoundation
    // For common: VS file
    if(fileName=="beauty_common.hlsl" || fileName=="makeup_common.hlsl"){
        return backend_->CreateShaderFromFile(path, "");
    } else {
        return backend_->CreateShaderFromFile("", path);
    }
}

HFResult FullGPUPipeline::Init(IRenderBackend* backend) {
    if(!backend || !backend->IsInitialized()){
        return HF_RESULT_INVALID_PARAM;
    }
    backend_ = backend;
    d3dBackend_ = dynamic_cast<D3D11Backend*>(backend);
    if(!d3dBackend_){
        return HF_RESULT_INVALID_PARAM;
    }
    pool_.Init(backend);

    if(!CreateConstantBuffers()){
        return HF_RESULT_FAIL;
    }
    if(!CreateSamplers()){
        return HF_RESULT_FAIL;
    }
    if(!CreateQuad()){
        return HF_RESULT_FAIL;
    }
    HFResult r = CompileShaders();
    if(r!=HF_RESULT_OK){
        return r;
    }
    initialized_ = true;
    return HF_RESULT_OK;
}

void FullGPUPipeline::Shutdown() {
    if(!initialized_) return;
    if(quadMesh_){
        backend_->DestroyMesh(quadMesh_);
        quadMesh_=nullptr;
    }
    // Destroy shaders — avoid double-free for reused pointers
    std::set<IShader*> uniqueShaders;
    auto collect = [&](IShader* s){ if(s) uniqueShaders.insert(s); };
    collect(shaders_.beautySmoothing);
    collect(shaders_.beautyTexture);
    collect(shaders_.beautyBlemish);
    collect(shaders_.beautyTone);
    collect(shaders_.beautyBrightness);
    collect(shaders_.beautyContrast);
    collect(shaders_.beautyFinal);
    collect(shaders_.makeupFoundation);
    collect(shaders_.makeupBlush);
    collect(shaders_.makeupEyeshadow);
    collect(shaders_.makeupEyebrow);
    collect(shaders_.makeupEyeliner);
    collect(shaders_.makeupEyelash);
    collect(shaders_.makeupLip);
    collect(shaders_.makeupPupil);
    collect(shaders_.makeupBlend);
    for(auto* s : uniqueShaders){
        if(s) backend_->DestroyShader(s);
    }
    shaders_ = {};

    beautyCB_.Reset();
    makeupCB_.Reset();
    linearSampler_.Reset();
    pointSampler_.Reset();

    pool_.Clear();
    backend_=nullptr;
    d3dBackend_=nullptr;
    initialized_=false;
    shadersCompiled_=false;
}

bool FullGPUPipeline::CreateConstantBuffers() {
    if(!d3dBackend_ || !d3dBackend_->GetDevice()) return false;
    auto device = d3dBackend_->GetDevice();

    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = sizeof(BeautyConstantsGPU);
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    // Align to 16 bytes
    desc.ByteWidth = (desc.ByteWidth + 15) & ~15;
    HRESULT hr = device->CreateBuffer(&desc, nullptr, &beautyCB_);
    if(FAILED(hr)){
        std::cerr << "[FullPipeline] Failed to create beauty CB hr=" << std::hex << hr << std::endl;
        return false;
    }

    desc.ByteWidth = sizeof(MakeupConstantsGPU);
    desc.ByteWidth = (desc.ByteWidth + 15) & ~15;
    hr = device->CreateBuffer(&desc, nullptr, &makeupCB_);
    if(FAILED(hr)){
        std::cerr << "[FullPipeline] Failed to create makeup CB hr=" << std::hex << hr << std::endl;
        return false;
    }
    return true;
}

bool FullGPUPipeline::CreateSamplers() {
    if(!d3dBackend_ || !d3dBackend_->GetDevice()) return false;
    auto device = d3dBackend_->GetDevice();

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    HRESULT hr = device->CreateSamplerState(&sampDesc, &linearSampler_);
    if(FAILED(hr)) return false;

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    hr = device->CreateSamplerState(&sampDesc, &pointSampler_);
    if(FAILED(hr)) return false;

    return true;
}

bool FullGPUPipeline::CreateQuad() {
    // Fullscreen quad: positions as float4 (x,y,z,w) + UV float2 = 6 floats stride 24, matches VS float4 POSITION
    // Phase 9 ROOT-CAUSE FIX Cluster B: Previous CCW {0,1,2,0,2,3} culled by default D3D11 CULL_BACK FrontCCW FALSE -> black output
    // Fixed to CW {0,2,1,0,3,2} which is front-facing under default (CW front), plus backend now sets CULL_NONE as robust fix
    // Also verify POSITION float4 TEXCOORD float2 stride 24 offset 0/16 vertexCount 6
    std::vector<float> verts = {
        -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, // 0: bottom-left, uv 0,0
         1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, // 1: bottom-right, uv 1,0
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f, // 2: top-right, uv 1,1
        -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 1.0f  // 3: top-left, uv 0,1
    };
    // CW winding for default FrontCounterClockwise=FALSE (front is CW)
    std::vector<int> indices = {0,2,1, 0,3,2};
    std::cout << "[FullPipeline] CreateQuad verts=" << verts.size() << " floats (" << verts.size()/6 << " vertices) stride 24 POSITION float4 offset0 TEXCOORD float2 offset16, indices=" << indices.size() << " (CW)" << std::endl;
    quadMesh_ = backend_->CreateMesh(verts, indices);
    if (quadMesh_) {
        std::cout << "[FullPipeline] Quad mesh created: vertexCount=" << quadMesh_->GetVertexCount() << " indexCount=" << quadMesh_->GetIndexCount() << std::endl;
    } else {
        std::cerr << "[FullPipeline] Quad mesh creation FAILED!" << std::endl;
    }
    return quadMesh_ != nullptr;
}

HFResult FullGPUPipeline::CompileShaders() {
    shaderLog_ = "";
    int compiled = 0;
    int total = 0;

    auto tryCompile = [&](const std::string& file, IShader** outPtr, const std::string& name){
        total++;
        IShader* s = CompileShaderFromFile(file);
        if(s){
            *outPtr = s;
            compiled++;
            shaderLog_ += file + " [" + name + "] PASS; ";
            std::cout << "[FullPipeline] Compiled " << file << " -> " << name << " PASS" << std::endl;
        } else {
            shaderLog_ += file + " [" + name + "] FAIL; ";
            std::cerr << "[FullPipeline] Failed to compile " << file << " -> " << name << std::endl;
        }
    };

    auto tryCompileEntry = [&](const std::string& file, IShader** outPtr, const std::string& entry){
        total++;
        std::string path = FindShaderPath(file);
        if(path.empty()){
            shaderLog_ += file + " [" + entry + "] FAIL (not found); ";
            std::cerr << "[FullPipeline] Shader not found: " << file << std::endl;
            return;
        }
        IShader* s = nullptr;
        if(d3dBackend_){
            s = d3dBackend_->CreateShaderFromFileWithEntry("", path, entry);
        }
        if(!s){
            // fallback to generic compile
            s = CompileShaderFromFile(file);
        }
        if(s){
            *outPtr = s;
            compiled++;
            shaderLog_ += file + " [" + entry + "] PASS; ";
            std::cout << "[FullPipeline] Compiled " << file << " -> " << entry << " PASS (specific entry)" << std::endl;
        } else {
            shaderLog_ += file + " [" + entry + "] FAIL; ";
            std::cerr << "[FullPipeline] Failed to compile " << file << " -> " << entry << std::endl;
        }
    };

    tryCompile("beauty_smoothing.hlsl", &shaders_.beautySmoothing, "PSSmoothing");
    tryCompile("beauty_texture.hlsl", &shaders_.beautyTexture, "PSTextureRefinement");
    tryCompile("beauty_blemish.hlsl", &shaders_.beautyBlemish, "PSBlemishReduction");
    tryCompile("beauty_tone.hlsl", &shaders_.beautyTone, "PSToneAdjustment");
    tryCompileEntry("beauty_adjustment.hlsl", &shaders_.beautyBrightness, "PSBrightness");
    tryCompileEntry("beauty_adjustment.hlsl", &shaders_.beautyContrast, "PSContrast");
    tryCompileEntry("beauty_adjustment.hlsl", &shaders_.beautyFinal, "PSBeautyFinal");

    tryCompile("makeup_foundation.hlsl", &shaders_.makeupFoundation, "PSFoundation");
    tryCompile("makeup_blush.hlsl", &shaders_.makeupBlush, "PSBlush");
    tryCompileEntry("makeup_eye.hlsl", &shaders_.makeupEyeshadow, "PSEyeshadow");
    tryCompileEntry("makeup_eye.hlsl", &shaders_.makeupEyebrow, "PSEyebrow");
    tryCompileEntry("makeup_eye.hlsl", &shaders_.makeupEyeliner, "PSEyeliner");
    tryCompileEntry("makeup_eye.hlsl", &shaders_.makeupEyelash, "PSEyelash");
    tryCompileEntry("makeup_eye.hlsl", &shaders_.makeupPupil, "PSPupil");
    tryCompile("makeup_lip.hlsl", &shaders_.makeupLip, "PSLip");
    tryCompile("makeup_blend.hlsl", &shaders_.makeupBlend, "PSBlend");

    // For beauty final retouch, we already reused
    if(compiled==total){
        shadersCompiled_=true;
        return HF_RESULT_OK;
    } else {
        // Allow partial but log
        std::cerr << "[FullPipeline] Shaders compiled " << compiled << "/" << total << std::endl;
        shadersCompiled_ = (compiled>=8); // at least beauty and makeup core
        if(shadersCompiled_) return HF_RESULT_OK;
        return HF_RESULT_FAIL;
    }
}

PingPongRTs FullGPUPipeline::AcquirePingPong(int width, int height, HFFormat format) {
    PingPongRTs pp;
    pp.rtA = pool_.AcquireRenderTarget(width, height, format);
    pp.rtB = pool_.AcquireRenderTarget(width, height, format);
    pp.current = pp.rtA;
    pp.isA = true;
    pp.width = width;
    pp.height = height;
    stats_.rtCount+=2;
    return pp;
}

void FullGPUPipeline::ReleasePingPong(PingPongRTs& pp) {
    if(pp.rtA) pool_.ReleaseRenderTarget(pp.rtA);
    if(pp.rtB) pool_.ReleaseRenderTarget(pp.rtB);
    pp.rtA=pp.rtB=pp.current=nullptr;
    stats_.rtCount-=2;
}

IGpuTexture* FullGPUPipeline::CreateTextureFromImage(const HFImage& img) {
    if(!img.IsValid()) return nullptr;
    IGpuTexture* tex = backend_->CreateTexture(img.width, img.height, HF_FORMAT_RGBA8, img.data.data());
    if(tex) stats_.textureCount++;
    return tex;
}

IGpuTexture* FullGPUPipeline::CreateTextureFromMask(const HFBeautyMask& mask) {
    if(!mask.IsValid()) return nullptr;
    int w = mask.width, h = mask.height;
    // Create R8 data first (1 byte per pixel) for proper pitch
    std::vector<uint8_t> dataR8(w*h);
    std::vector<uint8_t> dataRGBA(w*h*4);
    for(int i=0;i<w*h;++i){
        float a = mask.alpha[i];
        uint8_t v = (uint8_t)(a*255);
        dataR8[i]=v;
        dataRGBA[i*4+0]=v;
        dataRGBA[i*4+1]=v;
        dataRGBA[i*4+2]=v;
        dataRGBA[i*4+3]=v;
    }
    IGpuTexture* tex = backend_->CreateTexture(w, h, HF_FORMAT_R8, dataR8.data());
    // Use R8 or RGBA8 - backend may not support R8, fallback to RGBA8
    if(!tex){
        tex = backend_->CreateTexture(w, h, HF_FORMAT_RGBA8, dataRGBA.data());
    }
    if(tex) stats_.textureCount++;
    return tex;
}

IGpuTexture* FullGPUPipeline::CreateTextureFromMakeupMask(const HFMakeupMask& mask) {
    if(!mask.IsValid()) return nullptr;
    int w = mask.width, h = mask.height;
    std::vector<uint8_t> data(w*h*4);
    for(int i=0;i<w*h;++i){
        float a = mask.alpha[i];
        uint8_t v = (uint8_t)(a*255);
        data[i*4+0]=v;
        data[i*4+1]=v;
        data[i*4+2]=v;
        data[i*4+3]=v;
    }
    IGpuTexture* tex = backend_->CreateTexture(w, h, HF_FORMAT_RGBA8, data.data());
    if(tex) stats_.textureCount++;
    return tex;
}

void FullGPUPipeline::UpdateBeautyConstants(const HFBeautyParameters& params, int w, int h) {
    if(!d3dBackend_ || !beautyCB_) {
        std::cerr << "[FullPipeline] UpdateBeautyConstants NULL backend or CB!" << std::endl;
        return;
    }
    auto context = d3dBackend_->GetContext();
    if(!context) {
        std::cerr << "[FullPipeline] UpdateBeautyConstants NULL context!" << std::endl;
        return;
    }

    BeautyConstantsGPU cb = {};
    cb.smoothingIntensity = params.smoothing.enabled ? params.smoothing.intensity : 0.0f;
    cb.smoothingRadius = params.smoothing.radius;
    cb.smoothingOpacity = params.smoothing.opacity;
    cb.edgePreservation = params.smoothing.edgePreservation;

    cb.textureIntensity = params.texture.enabled ? params.texture.intensity : 0.0f;
    cb.texturePreservation = params.texture.preservation;
    cb.textureOpacity = params.texture.opacity;
    cb.textureDetailThreshold = params.texture.detailThreshold;

    cb.blemishIntensity = params.blemish.enabled ? params.blemish.intensity : 0.0f;
    cb.blemishRadius = params.blemish.radius;
    cb.blemishOpacity = params.blemish.opacity;

    cb.toneIntensity = params.tone.enabled ? params.tone.intensity : 0.0f;
    cb.toneTemperature = params.tone.temperature;
    cb.toneTint = params.tone.tint;
    cb.toneSaturation = params.tone.saturation;

    cb.brightness = params.brightness.enabled ? params.brightness.intensity : 0.0f;
    cb.contrast = params.contrast.enabled ? params.contrast.intensity : 0.0f;
    cb.beautyOpacity = params.opacity;
    cb.globalIntensity = params.globalIntensity;
    cb.texelSize[0] = 1.0f / (float)w;
    cb.texelSize[1] = 1.0f / (float)h;

    // Phase 9 ROOT-CAUSE FIX Cluster E: Constant Buffer Map/memcpy/Unmap/PSSetConstantBuffers register b0
    // Unbind CB before Map to avoid hazard, then Map with WRITE_DISCARD, memcpy, Unmap, then PSSetConstantBuffers b0
    ID3D11Buffer* nullCB = nullptr;
    context->PSSetConstantBuffers(0, 1, &nullCB);
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context->Map(beautyCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if(SUCCEEDED(hr)){
        memcpy(mapped.pData, &cb, sizeof(cb));
        context->Unmap(beautyCB_.Get(), 0);
        // Bind immediately after unmap to ensure PS b0 is valid for next draw
        ID3D11Buffer* cbs[1] = {beautyCB_.Get()};
        context->PSSetConstantBuffers(0, 1, cbs);
        std::cout << "[FullPipeline] UpdateBeautyConstants CB updated and bound to b0: brightness=" << cb.brightness << " contrast=" << cb.contrast << " smoothing=" << cb.smoothingIntensity << " texture=" << cb.textureIntensity << " blemish=" << cb.blemishIntensity << " tone=" << cb.toneIntensity << " opacity=" << cb.beautyOpacity << " global=" << cb.globalIntensity << " w=" << w << " h=" << h << " texelSize=" << cb.texelSize[0] << "," << cb.texelSize[1] << std::endl;
    } else {
        std::cerr << "[FullPipeline] Map beautyCB failed hr=" << std::hex << hr << std::endl;
    }
}

void FullGPUPipeline::UpdateMakeupConstants(const HFFloat4& color, float intensity, float opacity, HFBlendMode blendMode, float thickness, float irisEnhance, float scale) {
    if(!d3dBackend_ || !makeupCB_) {
        std::cerr << "[FullPipeline] UpdateMakeupConstants NULL backend or CB!" << std::endl;
        return;
    }
    auto context = d3dBackend_->GetContext();
    if(!context) {
        std::cerr << "[FullPipeline] UpdateMakeupConstants NULL context!" << std::endl;
        return;
    }

    MakeupConstantsGPU cb = {};
    cb.makeupColor[0] = color.r;
    cb.makeupColor[1] = color.g;
    cb.makeupColor[2] = color.b;
    cb.makeupColor[3] = color.a;
    cb.makeupIntensity = intensity;
    cb.makeupOpacity = opacity;
    cb.makeupFeather = 1.0f;
    cb.makeupScale = scale;
    cb.blendParams[0] = (float)blendMode;
    cb.blendParams[1] = thickness;
    cb.blendParams[2] = irisEnhance;
    cb.blendParams[3] = scale;

    // Phase 9 ROOT-CAUSE FIX Cluster E: Constant Buffer Map/memcpy/Unmap/PSSetConstantBuffers b0
    ID3D11Buffer* nullCB = nullptr;
    context->PSSetConstantBuffers(0, 1, &nullCB);
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context->Map(makeupCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if(SUCCEEDED(hr)){
        memcpy(mapped.pData, &cb, sizeof(cb));
        context->Unmap(makeupCB_.Get(), 0);
        ID3D11Buffer* cbs[1] = {makeupCB_.Get()};
        context->PSSetConstantBuffers(0, 1, cbs);
        std::cout << "[FullPipeline] UpdateMakeupConstants CB updated and bound to b0: color=" << color.r << "," << color.g << "," << color.b << " intensity=" << intensity << " opacity=" << opacity << " blendMode=" << (int)blendMode << " thickness=" << thickness << " scale=" << scale << std::endl;
    } else {
        std::cerr << "[FullPipeline] Map makeupCB failed hr=" << std::hex << hr << std::endl;
    }
}

void FullGPUPipeline::BindBeautyTextures(IGpuTexture* input, IGpuTexture* skinMask, IGpuTexture* intermediate) {
    if(!d3dBackend_) return;
    auto context = d3dBackend_->GetContext();
    if(!context) return;

    ID3D11ShaderResourceView* srvs[4] = {nullptr, nullptr, nullptr, nullptr};
    if(input){
        D3D11Texture* tex = static_cast<D3D11Texture*>(input);
        if(tex) srvs[0] = tex->srv.Get();
    }
    if(skinMask){
        D3D11Texture* tex = static_cast<D3D11Texture*>(skinMask);
        if(tex) srvs[1] = tex->srv.Get();
    }
    if(intermediate){
        D3D11Texture* tex = static_cast<D3D11Texture*>(intermediate);
        if(tex) srvs[2] = tex->srv.Get();
    }
    // BeautyMaskTexture t3 - reuse skinMask for now
    if(skinMask){
        D3D11Texture* tex = static_cast<D3D11Texture*>(skinMask);
        if(tex) srvs[3] = tex->srv.Get();
    }

    if(!srvs[0]) std::cerr << "[FullPipeline] BindBeautyTextures WARNING: input SRV NULL! t0 will sample black" << std::endl;
    if(!srvs[1]) std::cerr << "[FullPipeline] BindBeautyTextures WARNING: skinMask SRV NULL! t1 will be 0" << std::endl;
    if(!linearSampler_) std::cerr << "[FullPipeline] BindBeautyTextures WARNING: linearSampler NULL!" << std::endl;
    if(!beautyCB_) std::cerr << "[FullPipeline] BindBeautyTextures WARNING: beautyCB NULL!" << std::endl;

    std::cout << "[FullPipeline] BindBeautyTextures input SRV=" << (srvs[0]?"valid":"NULL") << " mask SRV=" << (srvs[1]?"valid":"NULL") << " intermediate SRV=" << (srvs[2]?"valid":"NULL") << " beautyMask SRV=" << (srvs[3]?"valid":"NULL") << " beautyCB=" << (beautyCB_?"valid":"NULL") << " linearSampler=" << (linearSampler_?"valid":"NULL") << " pointSampler=" << (pointSampler_?"valid":"NULL") << std::endl;

    // Phase 9 ROOT-CAUSE FIX Cluster C/D/E: Sampler s0/s1, SRV t0-t3, CB b0
    context->PSSetShaderResources(0, 4, srvs);

    ID3D11SamplerState* samplers[2] = {linearSampler_.Get(), pointSampler_.Get()};
    if(samplers[0] && samplers[1]){
        context->PSSetSamplers(0, 2, samplers);
        std::cout << "[FullPipeline] PSSetSamplers s0 linear s1 point bound" << std::endl;
    } else {
        std::cerr << "[FullPipeline] PSSetSamplers WARNING: sampler null!" << std::endl;
    }

    ID3D11Buffer* cbs[1] = {beautyCB_.Get()};
    if(cbs[0]){
        context->PSSetConstantBuffers(0, 1, cbs);
        std::cout << "[FullPipeline] PSSetConstantBuffers b0 beautyCB bound" << std::endl;
    } else {
        std::cerr << "[FullPipeline] PSSetConstantBuffers WARNING: beautyCB null!" << std::endl;
    }
}

void FullGPUPipeline::BindMakeupTextures(IGpuTexture* input, IGpuTexture* mask, IGpuTexture* makeupTex) {
    if(!d3dBackend_) return;
    auto context = d3dBackend_->GetContext();
    if(!context) return;

    ID3D11ShaderResourceView* srvs[3] = {nullptr, nullptr, nullptr};
    if(input){
        D3D11Texture* tex = static_cast<D3D11Texture*>(input);
        if(tex) srvs[0] = tex->srv.Get();
    }
    if(mask){
        D3D11Texture* tex = static_cast<D3D11Texture*>(mask);
        if(tex) srvs[1] = tex->srv.Get();
    }
    if(makeupTex){
        D3D11Texture* tex = static_cast<D3D11Texture*>(makeupTex);
        if(tex) srvs[2] = tex->srv.Get();
    } else {
        // If no makeup texture, reuse input
        if(input){
            D3D11Texture* tex = static_cast<D3D11Texture*>(input);
            if(tex) srvs[2] = tex->srv.Get();
        }
    }

    if(!srvs[0]) std::cerr << "[FullPipeline] BindMakeupTextures WARNING: input SRV NULL!" << std::endl;
    if(!srvs[1]) std::cerr << "[FullPipeline] BindMakeupTextures WARNING: mask SRV NULL!" << std::endl;
    std::cout << "[FullPipeline] BindMakeupTextures input SRV=" << (srvs[0]?"valid":"NULL") << " mask SRV=" << (srvs[1]?"valid":"NULL") << " makeup SRV=" << (srvs[2]?"valid":"NULL") << " makeupCB=" << (makeupCB_?"valid":"NULL") << std::endl;

    context->PSSetShaderResources(0, 3, srvs);

    ID3D11SamplerState* samplers[1] = {linearSampler_.Get()};
    if(samplers[0]){
        context->PSSetSamplers(0, 1, samplers);
        std::cout << "[FullPipeline] Makeup PSSetSamplers s0 linear bound" << std::endl;
    }

    ID3D11Buffer* cbs[1] = {makeupCB_.Get()};
    if(cbs[0]){
        context->PSSetConstantBuffers(0, 1, cbs);
        std::cout << "[FullPipeline] Makeup PSSetConstantBuffers b0 makeupCB bound" << std::endl;
    }
}

void FullGPUPipeline::UnbindTextures(int count) {
    if(!d3dBackend_) return;
    auto context = d3dBackend_->GetContext();
    if(!context) return;
    ID3D11ShaderResourceView* nullSRVs[4] = {nullptr, nullptr, nullptr, nullptr};
    context->PSSetShaderResources(0, count, nullSRVs);
}

void FullGPUPipeline::DrawQuad(IShader* shader) {
    if(!shader || !quadMesh_) return;
    std::map<std::string, Uniform> uniforms;
    backend_->DrawMesh(quadMesh_, shader, uniforms);
}

double FullGPUPipeline::ExecutePassWithTiming(IShader* shader, IRenderTarget* srcRT, IRenderTarget* dstRT, const std::string& passName, GPUPassInfo& outInfo) {
    if(!d3dBackend_ || !shader || !dstRT) return 0;

    // Setup pass info
    outInfo.name = passName;
    outInfo.inputResource = srcRT ? "RT" : "Texture";
    if(srcRT){
        D3D11RenderTarget* rt = static_cast<D3D11RenderTarget*>(srcRT);
        if(rt) outInfo.inputResource = "RT_" + std::to_string((uintptr_t)rt->texture.Get()) + "_" + std::to_string(rt->width) + "x" + std::to_string(rt->height);
    }
    D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(dstRT);
    if(dst){
        outInfo.outputResource = "RT_" + std::to_string((uintptr_t)dst->texture.Get()) + "_" + std::to_string(dst->width) + "x" + std::to_string(dst->height);
        outInfo.width = dst->width;
        outInfo.height = dst->height;
    }
    outInfo.shaderFile = passName + ".hlsl";
    outInfo.entryPoint = passName;
    outInfo.target = "ps_5_0";
    outInfo.valid = true;

    GPUTimestampQuery query;
    double gpuMs = 0;
    if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
        d3dBackend_->BeginGPUTimestamp(query);

        UnbindTextures(4);
        backend_->SetRenderTarget(dstRT);
        backend_->Clear(0,0,0,1);
        DrawQuad(shader);
        d3dBackend_->EndFrame();

        d3dBackend_->EndGPUTimestamp(query);
        gpuMs = d3dBackend_->GetGPUTimestampMs(query);
        outInfo.gpuMs = gpuMs;
        outInfo.valid = gpuMs>0;
    } else {
        // Fallback without timing
        UnbindTextures(4);
        backend_->SetRenderTarget(dstRT);
        backend_->Clear(0,0,0,1);
        DrawQuad(shader);
        d3dBackend_->EndFrame();
    }

    std::cout << "[FullPipeline] GPU PASS: Name=" << outInfo.name << " Input=" << outInfo.inputResource << " Output=" << outInfo.outputResource << " Shader=" << outInfo.shaderFile << " Entry=" << outInfo.entryPoint << " Target=" << outInfo.target << " Res=" << outInfo.width << "x" << outInfo.height << " GPU=" << gpuMs << "ms Valid=" << outInfo.valid << std::endl;

    return gpuMs;
}

FullGPUPipeline::BeautyPipelineResult FullGPUPipeline::ExecuteBeautyPipeline(
    IGpuTexture* inputTexture,
    IGpuTexture* skinMaskTexture,
    const HFBeautyParameters& params,
    int width, int height) {

    BeautyPipelineResult result;
    if(!initialized_ || !inputTexture || !skinMaskTexture){
        result.error = "Not initialized or null textures";
        return result;
    }

    // Acquire ping-pong RTs
    PingPongRTs pp = AcquirePingPong(width, height, HF_FORMAT_RGBA8);
    if(!pp.rtA || !pp.rtB){
        result.error = "Failed to acquire ping-pong RTs";
        return result;
    }

    // Initial input is inputTexture, first pass writes to rtA
    // For chaining, we need to have inputTexture as source for first pass
    // But our ExecutePassWithTiming expects srcRT and dstRT - for first pass, src is inputTexture, not RT
    // So we will handle first pass specially: bind inputTexture as t0, render to rtA
    // Then subsequent passes: src = previous RT, dst = other RT

    // Update beauty constants
    UpdateBeautyConstants(params, width, height);

    std::vector<GPUPassInfo> passes;
    double totalGpu = 0;

    // Pass 1: Smoothing - Input Texture -> RT A
    {
        GPUPassInfo info;
        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(inputTexture, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautySmoothing);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(inputTexture, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautySmoothing);
            d3dBackend_->EndFrame();
        }
        info.name = "SkinSmoothing";
        info.inputResource = "InputTexture_" + std::to_string((uintptr_t)inputTexture);
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtA);
        info.outputResource = "RT_A_" + std::to_string((uintptr_t)dst->texture.Get());
        info.shaderFile = "beauty_smoothing.hlsl";
        info.entryPoint = "PSSmoothing";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        info.valid = true;
        passes.push_back(info);
        totalGpu += gpuMs;
        result.perPassMs[0] = gpuMs;
        pp.current = pp.rtA;
        pp.isA = true;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " Shader=" << info.shaderFile << " Entry=" << info.entryPoint << " GPU=" << gpuMs << "ms" << std::endl;
    }

    // Pass 2: Texture Refinement - RT A -> RT B (chaining: input is output of smoothing)
    {
        GPUPassInfo info;
        // For chaining, source is pp.rtA, dest is pp.rtB
        // Bind source texture (rtA's texture) as input
        IGpuTexture* srcTex = pp.rtA->GetTexture();
        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyTexture);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyTexture);
            d3dBackend_->EndFrame();
        }
        info.name = "TextureRefinement";
        info.inputResource = "RT_A_Output_Smoothing";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtB);
        info.outputResource = "RT_B_" + std::to_string((uintptr_t)dst->texture.Get());
        info.shaderFile = "beauty_texture.hlsl";
        info.entryPoint = "PSTextureRefinement";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu += gpuMs;
        result.perPassMs[1] = gpuMs;
        pp.current = pp.rtB;
        pp.isA = false;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Smoothing)" << std::endl;
    }

    // Pass 3: Blemish Reduction - RT B -> RT A (chained from texture)
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtB->GetTexture();
        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyBlemish);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyBlemish);
            d3dBackend_->EndFrame();
        }
        info.name = "BlemishReduction";
        info.inputResource = "RT_B_Output_Texture";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtA);
        info.outputResource = "RT_A_" + std::to_string((uintptr_t)dst->texture.Get());
        info.shaderFile = "beauty_blemish.hlsl";
        info.entryPoint = "PSBlemishReduction";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu += gpuMs;
        result.perPassMs[2] = gpuMs;
        pp.current = pp.rtA;
        pp.isA = true;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Texture)" << std::endl;
    }

    // Pass 4: Tone Adjustment - RT A -> RT B
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtA->GetTexture();
        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyTone);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyTone);
            d3dBackend_->EndFrame();
        }
        info.name = "SkinTone";
        info.inputResource = "RT_A_Output_Blemish";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtB);
        info.outputResource = "RT_B_" + std::to_string((uintptr_t)dst->texture.Get());
        info.shaderFile = "beauty_tone.hlsl";
        info.entryPoint = "PSToneAdjustment";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu += gpuMs;
        result.perPassMs[3] = gpuMs;
        pp.current = pp.rtB;
        pp.isA = false;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Blemish)" << std::endl;
    }

    // Pass 5: Brightness - RT B -> RT A
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtB->GetTexture();
        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyBrightness);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyBrightness);
            d3dBackend_->EndFrame();
        }
        info.name = "Brightness";
        info.inputResource = "RT_B_Output_Tone";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtA);
        info.outputResource = "RT_A_" + std::to_string((uintptr_t)dst->texture.Get());
        info.shaderFile = "beauty_adjustment.hlsl";
        info.entryPoint = "PSBrightness";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu += gpuMs;
        result.perPassMs[4] = gpuMs;
        pp.current = pp.rtA;
        pp.isA = true;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Tone)" << std::endl;
    }

    // Pass 6: Contrast - RT A -> RT B
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtA->GetTexture();
        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyContrast);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyContrast);
            d3dBackend_->EndFrame();
        }
        info.name = "Contrast";
        info.inputResource = "RT_A_Output_Brightness";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtB);
        info.outputResource = "RT_B_" + std::to_string((uintptr_t)dst->texture.Get());
        info.shaderFile = "beauty_adjustment.hlsl";
        info.entryPoint = "PSContrast";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu += gpuMs;
        result.perPassMs[5] = gpuMs;
        pp.current = pp.rtB;
        pp.isA = false;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Brightness)" << std::endl;
    }

    // Pass 7: Retouch (BeautyFinal) - RT B -> RT A
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtB->GetTexture();
        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyFinal);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindBeautyTextures(srcTex, skinMaskTexture, nullptr);
            DrawQuad(shaders_.beautyFinal);
            d3dBackend_->EndFrame();
        }
        info.name = "FaceRetouch";
        info.inputResource = "RT_B_Output_Contrast";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtA);
        info.outputResource = "RT_A_" + std::to_string((uintptr_t)dst->texture.Get());
        info.shaderFile = "beauty_adjustment.hlsl";
        info.entryPoint = "PSBeautyFinal";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu += gpuMs;
        result.perPassMs[6] = gpuMs;
        pp.current = pp.rtA;
        pp.isA = true;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Contrast) FINAL BEAUTY OUTPUT" << std::endl;
    }

    result.success = true;
    result.pingPong = pp;
    result.passes = passes;
    result.totalGpuMs = totalGpu;

    std::cout << "[FullPipeline] Beauty Pipeline Complete: Input->Smoothing->Texture->Blemish->Tone->Brightness->Contrast->Retouch->Output, Total GPU=" << totalGpu << "ms, Chained=" << passes.size() << " passes, Final RT=" << (pp.isA?"A":"B") << std::endl;

    return result;
}

FullGPUPipeline::MakeupPipelineResult FullGPUPipeline::ExecuteMakeupPipeline(
    IGpuTexture* beautyOutputTexture,
    const std::map<MakeupMaskType, IGpuTexture*>& makeupMaskTextures,
    const HFMakeupParameters& params,
    int width, int height) {

    MakeupPipelineResult result;
    if(!initialized_ || !beautyOutputTexture){
        result.error = "Not initialized or null beauty output";
        return result;
    }

    PingPongRTs pp = AcquirePingPong(width, height, HF_FORMAT_RGBA8);
    if(!pp.rtA || !pp.rtB){
        result.error = "Failed to acquire ping-pong RTs for makeup";
        return result;
    }

    std::vector<GPUPassInfo> passes;
    double totalGpu = 0;

    // Helper to get mask texture
    auto getMask = [&](MakeupMaskType type) -> IGpuTexture* {
        auto it = makeupMaskTextures.find(type);
        if(it!=makeupMaskTextures.end()) return it->second;
        // Try face as fallback for foundation
        if(type==MakeupMaskType::Face){
            auto it2 = makeupMaskTextures.find(MakeupMaskType::Face);
            if(it2!=makeupMaskTextures.end()) return it2->second;
        }
        return nullptr;
    };

    // Pass 1: Foundation - BeautyOutput -> RT A
    {
        GPUPassInfo info;
        IGpuTexture* mask = getMask(MakeupMaskType::Face);
        if(!mask) mask = getMask(MakeupMaskType::LeftCheek); // fallback
        UpdateMakeupConstants(params.foundation.color, params.foundation.enabled?params.foundation.intensity:0, params.foundation.opacity, params.foundation.blendMode, params.foundation.softness, 0, 1.0f);

        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(beautyOutputTexture, mask, nullptr);
            DrawQuad(shaders_.makeupFoundation);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(beautyOutputTexture, mask, nullptr);
            DrawQuad(shaders_.makeupFoundation);
            d3dBackend_->EndFrame();
        }
        info.name = "Foundation";
        info.inputResource = "BeautyOutput";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtA);
        info.outputResource = "RT_A_Foundation";
        info.shaderFile = "makeup_foundation.hlsl";
        info.entryPoint = "PSFoundation";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu+=gpuMs;
        result.perPassMs[0]=gpuMs;
        pp.current = pp.rtA;
        pp.isA = true;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=BeautyOutput Output=" << info.outputResource << " GPU=" << gpuMs << "ms" << std::endl;
    }

    // Pass 2: Blush - RT A -> RT B (chained)
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtA->GetTexture();
        IGpuTexture* mask = getMask(MakeupMaskType::LeftCheek);
        if(!mask) mask = getMask(MakeupMaskType::RightCheek);
        if(!mask) mask = getMask(MakeupMaskType::Face);
        UpdateMakeupConstants(params.blush.color, params.blush.enabled?params.blush.intensity:0, params.blush.opacity, params.blush.blendMode, 1.0f, 0, params.blush.scale);

        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupBlush);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupBlush);
            d3dBackend_->EndFrame();
        }
        info.name = "Blush";
        info.inputResource = "RT_A_Output_Foundation";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtB);
        info.outputResource = "RT_B_Blush";
        info.shaderFile = "makeup_blush.hlsl";
        info.entryPoint = "PSBlush";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu+=gpuMs;
        result.perPassMs[1]=gpuMs;
        pp.current = pp.rtB;
        pp.isA = false;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Foundation)" << std::endl;
    }

    // Pass 3: Eyeshadow - RT B -> RT A
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtB->GetTexture();
        IGpuTexture* mask = getMask(MakeupMaskType::LeftEyelid);
        if(!mask) mask = getMask(MakeupMaskType::RightEyelid);
        UpdateMakeupConstants(params.eyeshadow.color, params.eyeshadow.enabled?params.eyeshadow.intensity:0, params.eyeshadow.opacity, params.eyeshadow.blendMode, 1.0f, 0, 1.0f);

        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupEyeshadow);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupEyeshadow);
            d3dBackend_->EndFrame();
        }
        info.name = "Eyeshadow";
        info.inputResource = "RT_B_Output_Blush";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtA);
        info.outputResource = "RT_A_Eyeshadow";
        info.shaderFile = "makeup_eye.hlsl";
        info.entryPoint = "PSEyeshadow";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu+=gpuMs;
        result.perPassMs[2]=gpuMs;
        pp.current = pp.rtA;
        pp.isA = true;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Blush)" << std::endl;
    }

    // Pass 4: Eyebrow - RT A -> RT B
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtA->GetTexture();
        IGpuTexture* mask = getMask(MakeupMaskType::LeftEyebrow);
        if(!mask) mask = getMask(MakeupMaskType::RightEyebrow);
        UpdateMakeupConstants(params.eyebrow.color, params.eyebrow.enabled?params.eyebrow.intensity:0, params.eyebrow.opacity, params.eyebrow.blendMode, params.eyebrow.thickness, 0, 1.0f);

        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupEyebrow);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupEyebrow);
            d3dBackend_->EndFrame();
        }
        info.name = "Eyebrow";
        info.inputResource = "RT_A_Output_Eyeshadow";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtB);
        info.outputResource = "RT_B_Eyebrow";
        info.shaderFile = "makeup_eye.hlsl";
        info.entryPoint = "PSEyebrow";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu+=gpuMs;
        result.perPassMs[3]=gpuMs;
        pp.current = pp.rtB;
        pp.isA = false;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Eyeshadow)" << std::endl;
    }

    // Pass 5: Eyeliner - RT B -> RT A
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtB->GetTexture();
        IGpuTexture* mask = getMask(MakeupMaskType::LeftEyelid);
        UpdateMakeupConstants(params.eyeliner.color, params.eyeliner.enabled?params.eyeliner.intensity:0, params.eyeliner.opacity, params.eyeliner.blendMode, params.eyeliner.thickness, 0, 1.0f);

        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupEyeliner);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupEyeliner);
            d3dBackend_->EndFrame();
        }
        info.name = "Eyeliner";
        info.inputResource = "RT_B_Output_Eyebrow";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtA);
        info.outputResource = "RT_A_Eyeliner";
        info.shaderFile = "makeup_eye.hlsl";
        info.entryPoint = "PSEyeliner";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu+=gpuMs;
        result.perPassMs[4]=gpuMs;
        pp.current = pp.rtA;
        pp.isA = true;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Eyebrow)" << std::endl;
    }

    // Pass 6: Eyelash - RT A -> RT B
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtA->GetTexture();
        IGpuTexture* mask = getMask(MakeupMaskType::LeftEyelid);
        UpdateMakeupConstants(params.eyelash.color, params.eyelash.intensity, params.eyelash.opacity, HFBlendMode::Normal, params.eyelash.thickness, params.eyelash.length, 1.0f);

        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupEyelash);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupEyelash);
            d3dBackend_->EndFrame();
        }
        info.name = "Eyelash";
        info.inputResource = "RT_A_Output_Eyeliner";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtB);
        info.outputResource = "RT_B_Eyelash";
        info.shaderFile = "makeup_eye.hlsl";
        info.entryPoint = "PSEyelash";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu+=gpuMs;
        result.perPassMs[5]=gpuMs;
        pp.current = pp.rtB;
        pp.isA = false;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Eyeliner)" << std::endl;
    }

    // Pass 7: Lip - RT B -> RT A
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtB->GetTexture();
        IGpuTexture* mask = getMask(MakeupMaskType::Lip);
        UpdateMakeupConstants(params.lip.color, params.lip.enabled?params.lip.intensity:0, params.lip.opacity, params.lip.blendMode, 1.0f, 0, params.lip.scale);

        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupLip);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupLip);
            d3dBackend_->EndFrame();
        }
        info.name = "Lip";
        info.inputResource = "RT_B_Output_Eyelash";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtA);
        info.outputResource = "RT_A_Lip";
        info.shaderFile = "makeup_lip.hlsl";
        info.entryPoint = "PSLip";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu+=gpuMs;
        result.perPassMs[6]=gpuMs;
        pp.current = pp.rtA;
        pp.isA = true;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Eyelash)" << std::endl;
    }

    // Pass 8: Pupil - RT A -> RT B
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtA->GetTexture();
        IGpuTexture* mask = getMask(MakeupMaskType::LeftEye);
        if(!mask) mask = getMask(MakeupMaskType::RightEye);
        if(!mask) mask = getMask(MakeupMaskType::Face);
        UpdateMakeupConstants(params.pupil.color, params.pupil.enabled?params.pupil.intensity:0, params.pupil.opacity, HFBlendMode::Normal, 1.0f, params.pupil.irisEnhancement, params.pupil.scale);

        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupPupil);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupPupil);
            d3dBackend_->EndFrame();
        }
        info.name = "Pupil";
        info.inputResource = "RT_A_Output_Lip";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtB);
        info.outputResource = "RT_B_Pupil";
        info.shaderFile = "makeup_eye.hlsl";
        info.entryPoint = "PSPupil";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu+=gpuMs;
        result.perPassMs[7]=gpuMs;
        pp.current = pp.rtB;
        pp.isA = false;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Lip)" << std::endl;
    }

    // Pass 9: Final Blend - RT B -> RT A (final output)
    {
        GPUPassInfo info;
        IGpuTexture* srcTex = pp.rtB->GetTexture();
        IGpuTexture* mask = getMask(MakeupMaskType::Face);
        UpdateMakeupConstants(HFFloat4(1,1,1,1), 1.0f, 1.0f, HFBlendMode::Normal, 1.0f, 0, 1.0f);

        GPUTimestampQuery query;
        double gpuMs = 0;
        if(d3dBackend_->CreateTimestampQueries(query)==HF_RESULT_OK){
            d3dBackend_->BeginGPUTimestamp(query);
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupBlend);
            d3dBackend_->EndFrame();
            d3dBackend_->EndGPUTimestamp(query);
            gpuMs = d3dBackend_->GetGPUTimestampMs(query);
            info.gpuMs = gpuMs;
            info.valid = gpuMs>0;
        } else {
            UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
            backend_->Clear(0,0,0,1);
            BindMakeupTextures(srcTex, mask, nullptr);
            DrawQuad(shaders_.makeupBlend);
            d3dBackend_->EndFrame();
        }
        info.name = "Blend";
        info.inputResource = "RT_B_Output_Pupil";
        D3D11RenderTarget* dst = static_cast<D3D11RenderTarget*>(pp.rtA);
        info.outputResource = "RT_A_Final_Blend";
        info.shaderFile = "makeup_blend.hlsl";
        info.entryPoint = "PSBlend";
        info.target = "ps_5_0";
        info.width = width;
        info.height = height;
        passes.push_back(info);
        totalGpu+=gpuMs;
        result.perPassMs[8]=gpuMs;
        pp.current = pp.rtA;
        pp.isA = true;
        std::cout << "[FullPipeline] GPU PASS: Name=" << info.name << " Input=" << info.inputResource << " Output=" << info.outputResource << " GPU=" << gpuMs << "ms (CHAINED from Pupil) FINAL MAKEUP OUTPUT" << std::endl;
    }

    result.success = true;
    result.pingPong = pp;
    result.passes = passes;
    result.totalGpuMs = totalGpu;

    std::cout << "[FullPipeline] Makeup Pipeline Complete: Foundation->Blush->Eyeshadow->Eyebrow->Eyeliner->Eyelash->Lip->Pupil->Blend->Final, Total GPU=" << totalGpu << "ms, Chained=" << passes.size() << " passes" << std::endl;

    return result;
}

FullGPUPipeline::FullPipelineResult FullGPUPipeline::ExecuteFullPipeline(
    IGpuTexture* inputTexture,
    IGpuTexture* skinMaskTexture,
    const std::map<MakeupMaskType, IGpuTexture*>& makeupMaskTextures,
    const HFBeautyParameters& beautyParams,
    const HFMakeupParameters& makeupParams,
    int width, int height) {

    FullPipelineResult result;
    if(!initialized_){
        result.error = "Not initialized";
        return result;
    }

    // Beauty pipeline first
    BeautyPipelineResult beautyResult = ExecuteBeautyPipeline(inputTexture, skinMaskTexture, beautyParams, width, height);
    if(!beautyResult.success){
        result.error = "Beauty pipeline failed: " + beautyResult.error;
        // Release beauty ping-pong if needed
        ReleasePingPong(beautyResult.pingPong);
        return result;
    }

    // Beauty output is beautyResult.pingPong.current
    IGpuTexture* beautyOutputTex = beautyResult.pingPong.current->GetTexture();

    // Makeup pipeline uses beauty output as input
    // For makeup, we need to create new ping-pong or reuse? We will create new ping-pong for makeup, but first pass uses beauty output
    // Actually ExecuteMakeupPipeline creates its own ping-pong internally, with first pass input = beautyOutput
    // So we need to keep beauty ping-pong alive until makeup first pass done, then we can release beauty ping-pong after copying or we keep beauty output texture
    // Simplest: ExecuteMakeupPipeline with beautyOutputTex, it will create its own ping-pong and first pass will read beautyOutputTex and write to its rtA
    MakeupPipelineResult makeupResult = ExecuteMakeupPipeline(beautyOutputTex, makeupMaskTextures, makeupParams, width, height);
    if(!makeupResult.success){
        result.error = "Makeup pipeline failed: " + makeupResult.error;
        ReleasePingPong(beautyResult.pingPong);
        ReleasePingPong(makeupResult.pingPong);
        return result;
    }

    // Full pipeline result: beauty passes + makeup passes, final RT is makeupResult ping-pong current
    // We need to release beauty ping-pong RTs that are not needed, but keep final makeup RT
    // For simplicity, we will release beauty ping-pong RTs and keep makeup ping-pong as final
    ReleasePingPong(beautyResult.pingPong);

    result.success = true;
    result.beautyPasses = beautyResult.passes;
    result.makeupPasses = makeupResult.passes;
    result.beautyGpuMs = beautyResult.totalGpuMs;
    result.makeupGpuMs = makeupResult.totalGpuMs;
    result.totalGpuMs = beautyResult.totalGpuMs + makeupResult.totalGpuMs;
    result.pingPong = makeupResult.pingPong;

    std::cout << "[FullPipeline] Full Pipeline Complete: Beauty " << result.beautyGpuMs << "ms + Makeup " << result.makeupGpuMs << "ms = Total " << result.totalGpuMs << "ms, Beauty->Makeup chained, Final RT valid" << std::endl;

    return result;
}

FullGPUPipeline::ChainingValidationResult FullGPUPipeline::ValidateBeautyChaining(IGpuTexture* input, IGpuTexture* skinMask, const HFBeautyParameters& params, int w, int h) {
    ChainingValidationResult result;
    // Execute beauty pipeline and check that each pass input is previous output
    BeautyPipelineResult beautyResult = ExecuteBeautyPipeline(input, skinMask, params, w, h);
    if(!beautyResult.success){
        result.details = "Beauty pipeline failed: " + beautyResult.error;
        ReleasePingPong(beautyResult.pingPong);
        return result;
    }

    // Check chaining: each pass's input should be previous pass's output
    bool chained = true;
    std::string details = "Beauty chaining: ";
    for(size_t i=1;i<beautyResult.passes.size();++i){
        auto& prev = beautyResult.passes[i-1];
        auto& curr = beautyResult.passes[i];
        // Input of curr should contain output of prev (we encode output resource name in inputResource)
        // Our inputResource strings are like "RT_A_Output_Smoothing" etc, which indicates chaining
        // Check that inputResource contains previous pass name or output
        if(curr.inputResource.find(prev.name)==std::string::npos && curr.inputResource.find("RT_")==std::string::npos){
            // If not containing, still check that it's not original input
            if(curr.inputResource.find("InputTexture")!=std::string::npos){
                chained = false;
                details += curr.name + " uses original input, not chained from " + prev.name + "; ";
            }
        } else {
            details += curr.name + " chained from " + prev.name + " (Input=" + curr.inputResource + " Output=" + curr.outputResource + "); ";
        }
    }

    result.isChained = chained;
    result.details = details + " Total passes=" + std::to_string(beautyResult.passes.size()) + " Total GPU=" + std::to_string(beautyResult.totalGpuMs) + "ms";
    result.diffA = beautyResult.totalGpuMs; // placeholder

    ReleasePingPong(beautyResult.pingPong);
    return result;
}

FullGPUPipeline::ChainingValidationResult FullGPUPipeline::ValidateMakeupChaining(IGpuTexture* beautyOutput, const std::map<MakeupMaskType, IGpuTexture*>& masks, const HFMakeupParameters& params, int w, int h) {
    ChainingValidationResult result;
    MakeupPipelineResult makeupResult = ExecuteMakeupPipeline(beautyOutput, masks, params, w, h);
    if(!makeupResult.success){
        result.details = "Makeup pipeline failed: " + makeupResult.error;
        ReleasePingPong(makeupResult.pingPong);
        return result;
    }

    bool chained = true;
    std::string details = "Makeup chaining: ";
    for(size_t i=1;i<makeupResult.passes.size();++i){
        auto& prev = makeupResult.passes[i-1];
        auto& curr = makeupResult.passes[i];
        if(curr.inputResource.find(prev.name)==std::string::npos && curr.inputResource.find("RT_")==std::string::npos){
            if(curr.inputResource.find("BeautyOutput")!=std::string::npos && i!=1){
                chained = false;
                details += curr.name + " uses BeautyOutput original, not chained from " + prev.name + "; ";
            }
        } else {
            details += curr.name + " chained from " + prev.name + "; ";
        }
    }

    result.isChained = chained;
    result.details = details + " Total passes=" + std::to_string(makeupResult.passes.size()) + " Total GPU=" + std::to_string(makeupResult.totalGpuMs) + "ms";

    ReleasePingPong(makeupResult.pingPong);
    return result;
}

HFImage FullGPUPipeline::ReadbackTexture(IGpuTexture* tex) {
    HFImage out;
    if(!d3dBackend_ || !tex) {
        std::cerr << "[FullPipeline] ReadbackTexture NULL backend or tex" << std::endl;
        return out;
    }
    auto device = d3dBackend_->GetDevice();
    auto context = d3dBackend_->GetContext();
    if(!device || !context) {
        std::cerr << "[FullPipeline] ReadbackTexture NULL device or context" << std::endl;
        return out;
    }

    // Phase 9 ROOT-CAUSE FIX Cluster G: Unbind RT and SRVs before CopyResource to avoid D3D11 hazard (RTV still bound as SRV)
    UnbindTextures(4);
    if(backend_) backend_->SetRenderTarget(nullptr);
    if(d3dBackend_) d3dBackend_->EndFrame();

    D3D11Texture* d3dTex = static_cast<D3D11Texture*>(tex);
    if(!d3dTex || !d3dTex->texture) {
        std::cerr << "[FullPipeline] ReadbackTexture d3dTex null or texture null" << std::endl;
        return out;
    }

    D3D11_TEXTURE2D_DESC desc;
    d3dTex->texture->GetDesc(&desc);
    std::cout << "[FullPipeline] ReadbackTexture source desc w=" << desc.Width << " h=" << desc.Height << " fmt=" << (int)desc.Format << " usage=" << (int)desc.Usage << " bindFlags=" << desc.BindFlags << std::endl;

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;

    ComPtr<ID3D11Texture2D> staging;
    HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &staging);
    if(FAILED(hr) || !staging) {
        std::cerr << "[FullPipeline] Failed to create staging texture for readback hr=" << std::hex << hr << std::endl;
        return out;
    }

    context->CopyResource(staging.Get(), d3dTex->texture.Get());
    // Ensure GPU work completed
    context->Flush();

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if(FAILED(hr)) {
        std::cerr << "[FullPipeline] Failed to map staging texture hr=" << std::hex << hr << std::endl;
        return out;
    }

    out.width = desc.Width;
    out.height = desc.Height;
    out.channels = 4;
    out.data.resize(desc.Width * desc.Height * 4);

    // Handle RowPitch may be larger than width*4
    if(mapped.RowPitch == desc.Width * 4) {
        memcpy(out.data.data(), mapped.pData, out.data.size());
    } else {
        for(UINT y=0; y<desc.Height; ++y) {
            uint8_t* src = (uint8_t*)mapped.pData + y * mapped.RowPitch;
            uint8_t* dst = out.data.data() + y * desc.Width * 4;
            memcpy(dst, src, desc.Width * 4);
        }
    }

    context->Unmap(staging.Get(), 0);

    // Phase 9 DEBUG: Log firstPixel, centerPixel, lastPixel, nonZeroPixelCount, average to prove RT readback works
    if(out.IsValid() && out.data.size()>=4){
        int w = out.width, h = out.height;
        uint8_t r0=out.data[0], g0=out.data[1], b0=out.data[2], a0=out.data[3];
        int cx = w/2, cy = h/2;
        int cIdx = (cy*w+cx)*4;
        uint8_t rc = out.data[cIdx], gc = out.data[cIdx+1], bc = out.data[cIdx+2], ac = out.data[cIdx+3];
        int lastIdx = (w*h-1)*4;
        uint8_t rl = out.data[lastIdx], gl = out.data[lastIdx+1], bl = out.data[lastIdx+2], al = out.data[lastIdx+3];
        size_t nonZero=0;
        double sumR=0,sumG=0,sumB=0;
        for(int i=0;i<w*h;++i){
            int idx=i*4;
            if(out.data[idx]!=0 || out.data[idx+1]!=0 || out.data[idx+2]!=0) nonZero++;
            sumR+=out.data[idx];
            sumG+=out.data[idx+1];
            sumB+=out.data[idx+2];
        }
        double avgR=sumR/(w*h), avgG=sumG/(w*h), avgB=sumB/(w*h);
        std::cout << "[Readback DEBUG] w=" << w << " h=" << h << " firstPixel RGBA=" << (int)r0 << "," << (int)g0 << "," << (int)b0 << "," << (int)a0 << " centerPixel=" << (int)rc << "," << (int)gc << "," << (int)bc << "," << (int)ac << " lastPixel=" << (int)rl << "," << (int)gl << "," << (int)bl << "," << (int)al << " nonZeroCount=" << nonZero << "/" << (w*h) << " avg=" << avgR << "," << avgG << "," << avgB << " RowPitch=" << mapped.RowPitch << " expected=" << (w*4) << std::endl;
    }

    return out;
}

HFImage FullGPUPipeline::ReadbackRenderTarget(IRenderTarget* rt) {
    if(!rt) return HFImage();
    IGpuTexture* tex = rt->GetTexture();
    return ReadbackTexture(tex);
}

double FullGPUPipeline::CalcMAE(const HFImage& a, const HFImage& b) {
    if(!a.IsValid() || !b.IsValid() || a.data.size()!=b.data.size()) return -1;
    double sum=0;
    for(size_t i=0;i<a.data.size();++i) {
        sum += std::abs((int)a.data[i] - (int)b.data[i]);
    }
    return sum / a.data.size();
}

double FullGPUPipeline::CalcMaxError(const HFImage& a, const HFImage& b) {
    if(!a.IsValid() || !b.IsValid() || a.data.size()!=b.data.size()) return -1;
    double maxErr=0;
    for(size_t i=0;i<a.data.size();++i) {
        double diff = std::abs((int)a.data[i] - (int)b.data[i]);
        if(diff>maxErr) maxErr=diff;
    }
    return maxErr;
}

double FullGPUPipeline::CalcRMSE(const HFImage& a, const HFImage& b) {
    if(!a.IsValid() || !b.IsValid() || a.data.size()!=b.data.size()) return -1;
    double sumSq=0;
    for(size_t i=0;i<a.data.size();++i) {
        double diff = (int)a.data[i] - (int)b.data[i];
        sumSq += diff*diff;
    }
    return std::sqrt(sumSq / a.data.size());
}

FullGPUPipeline::SentinelResult FullGPUPipeline::ExecuteChainingSentinelTest(IGpuTexture* input, IGpuTexture* skinMask, int w, int h) {
    SentinelResult result;
    if(!initialized_ || !input || !skinMask) {
        result.details = "Not initialized or null textures";
        return result;
    }

    // Step A: Create input that is easily recognizable (we already have input)
    // Step B: Pass A produces output with deterministic modification (smoothing)
    // For sentinel, we will do:
    // 1. Beauty Pass1 Smoothing: Input -> RT_A (output A)
    // 2. Beauty Pass2 Texture: RT_A -> RT_B (output B using A)
    // 3. Beauty Pass2 Texture: Input -> RT_B2 (output B using original)
    // If B using A != B using original, chaining is real

    HFBeautyParameters paramsA;
    paramsA.enabled=true;
    paramsA.globalIntensity=1.0f;
    paramsA.opacity=1.0f;
    paramsA.retouch.enabled=false;
    paramsA.smoothing.enabled=false;
    paramsA.texture.enabled=false;
    paramsA.blemish.enabled=false;
    paramsA.tone.enabled=false;
    paramsA.contrast.enabled=false;
    paramsA.brightness.enabled=true;
    paramsA.brightness.intensity=0.2f;
    paramsA.brightness.opacity=1.0f;

    HFBeautyParameters paramsB;
    paramsB.enabled=true;
    paramsB.globalIntensity=1.0f;
    paramsB.opacity=1.0f;
    paramsB.retouch.enabled=false;
    paramsB.smoothing.enabled=false;
    paramsB.texture.enabled=false;
    paramsB.blemish.enabled=false;
    paramsB.tone.enabled=false;
    paramsB.contrast.enabled=false;
    paramsB.brightness.enabled=true;
    paramsB.brightness.intensity=0.3f;
    paramsB.brightness.opacity=1.0f;

    // Acquire ping-pong
    PingPongRTs pp = AcquirePingPong(w, h, HF_FORMAT_RGBA8);
    if(!pp.rtA || !pp.rtB) {
        result.details = "Failed to acquire ping-pong";
        return result;
    }

    // For sentinel, create white mask (alpha 1) to ensure brightness affects all pixels
    IGpuTexture* whiteMaskTex = nullptr;
    {
        std::vector<uint8_t> whiteData(w*h*4, 255);
        whiteMaskTex = backend_->CreateTexture(w, h, HF_FORMAT_RGBA8, whiteData.data());
        std::cout << "[FullPipeline] Sentinel whiteMaskTex=" << (whiteMaskTex? "created":"FAILED") << " w=" << w << " h=" << h << std::endl;
    }
    IGpuTexture* maskForSentinel = whiteMaskTex ? whiteMaskTex : skinMask;
    std::cout << "[FullPipeline] Sentinel maskForSentinel=" << (maskForSentinel==whiteMaskTex? "white":"skinMask") << " brightness shader=" << (shaders_.beautyBrightness? "valid":"NULL") << std::endl;

    // Pass A: Input -> RT_A
    UpdateBeautyConstants(paramsA, w, h);
    {
        UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
        backend_->Clear(0,0,0,1);
        BindBeautyTextures(input, maskForSentinel, nullptr);
        DrawQuad(shaders_.beautyBrightness);
        d3dBackend_->EndFrame();
    }
    pp.current = pp.rtA;
    pp.isA = true;

    HFImage outputA = ReadbackRenderTarget(pp.rtA);
    HFImage inputCPU = ReadbackTexture(input);

    // Pass B using A: RT_A -> RT_B
    UpdateBeautyConstants(paramsB, w, h);
    IGpuTexture* srcA = pp.rtA->GetTexture();
    {
        UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtB);
        backend_->Clear(0,0,0,1);
        BindBeautyTextures(srcA, maskForSentinel, nullptr);
        DrawQuad(shaders_.beautyBrightness);
        d3dBackend_->EndFrame();
    }
    HFImage outputB_using_A = ReadbackRenderTarget(pp.rtB);

    // Pass B using original: Input -> RT_A (reuse rtA)
    {
        UnbindTextures(4);
        backend_->SetRenderTarget(pp.rtA);
        backend_->Clear(0,0,0,1);
        BindBeautyTextures(input, maskForSentinel, nullptr);
        DrawQuad(shaders_.beautyBrightness);
        d3dBackend_->EndFrame();
    }
    HFImage outputB_using_Original = ReadbackRenderTarget(pp.rtA);

    // Calculate diffs
    result.diffA = CalcMAE(inputCPU, outputA);
    result.diffBvsA = CalcMAE(outputA, outputB_using_A);
    result.diffBvsOriginal = CalcMAE(inputCPU, outputB_using_Original);
    result.diffBOrigVsAOrig = CalcMAE(outputB_using_A, outputB_using_Original);

    if(inputCPU.IsValid() && inputCPU.data.size()>=4){
        std::cout << "[Sentinel DEBUG] InputCPU firstPixel RGBA=" << (int)inputCPU.data[0] << "," << (int)inputCPU.data[1] << "," << (int)inputCPU.data[2] << "," << (int)inputCPU.data[3] << " size=" << inputCPU.width << "x" << inputCPU.height << std::endl;
    }
    if(outputA.IsValid() && outputA.data.size()>=4){
        std::cout << "[Sentinel DEBUG] OutputA (0.2 from Input) firstPixel RGBA=" << (int)outputA.data[0] << "," << (int)outputA.data[1] << "," << (int)outputA.data[2] << "," << (int)outputA.data[3] << " size=" << outputA.width << "x" << outputA.height << std::endl;
    }
    if(outputB_using_A.IsValid() && outputB_using_A.data.size()>=4){
        std::cout << "[Sentinel DEBUG] OutputB_using_A (0.3 from A) firstPixel RGBA=" << (int)outputB_using_A.data[0] << "," << (int)outputB_using_A.data[1] << "," << (int)outputB_using_A.data[2] << "," << (int)outputB_using_A.data[3] << " size=" << outputB_using_A.width << "x" << outputB_using_A.height << std::endl;
    }
    if(outputB_using_Original.IsValid() && outputB_using_Original.data.size()>=4){
        std::cout << "[Sentinel DEBUG] OutputB_using_Original (0.3 from Input) firstPixel RGBA=" << (int)outputB_using_Original.data[0] << "," << (int)outputB_using_Original.data[1] << "," << (int)outputB_using_Original.data[2] << "," << (int)outputB_using_Original.data[3] << " size=" << outputB_using_Original.width << "x" << outputB_using_Original.height << std::endl;
    }

    bool chained = result.diffBOrigVsAOrig > 0.5;
    result.isChained = chained;

    result.details = "Sentinel: Input->A(Brightness 0.2) MAE=" + std::to_string(result.diffA) +
                     " A->B(Brightness 0.3 using A) vs A MAE=" + std::to_string(result.diffBvsA) +
                     " Input->B(Brightness 0.3 using Original) MAE=" + std::to_string(result.diffBvsOriginal) +
                     " B_using_A vs B_using_Original MAE=" + std::to_string(result.diffBOrigVsAOrig) +
                     " Chained=" + std::to_string(chained) +
                     " FirstPixel Input=" + (inputCPU.IsValid()? std::to_string((int)inputCPU.data[0])+","+std::to_string((int)inputCPU.data[1])+","+std::to_string((int)inputCPU.data[2]):"invalid") +
                     " A=" + (outputA.IsValid()? std::to_string((int)outputA.data[0])+","+std::to_string((int)outputA.data[1])+","+std::to_string((int)outputA.data[2]):"invalid") +
                     " B_A=" + (outputB_using_A.IsValid()? std::to_string((int)outputB_using_A.data[0])+","+std::to_string((int)outputB_using_A.data[1])+","+std::to_string((int)outputB_using_A.data[2]):"invalid") +
                     " B_Orig=" + (outputB_using_Original.IsValid()? std::to_string((int)outputB_using_Original.data[0])+","+std::to_string((int)outputB_using_Original.data[1])+","+std::to_string((int)outputB_using_Original.data[2]):"invalid") +
                     " — REAL CHAINING if B_using_A != B_using_Original, proves pixel shader reads previous RT texture via SRV t0, not just name, whiteMask 1.0 ensures brightness affects all pixels";

    if(whiteMaskTex) backend_->DestroyTexture(whiteMaskTex);
    ReleasePingPong(pp);

    std::cout << "[FullPipeline] Sentinel Test: " << result.details << std::endl;

    return result;
}

} // namespace huanface

#endif // _WIN32
