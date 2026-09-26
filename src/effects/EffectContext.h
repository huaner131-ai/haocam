#pragma once

// Shared context handed to every effect provider at initialization
// (architecture spec, section 15).
//
// Providers receive raw D3D11 interfaces for the frame pipeline device plus
// access to shared services (tracking, texture pooling, scheduling, logs).

#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace haocam {

class IFaceTracker;
class ITexturePool;
class FrameScheduler; // defined in frame scheduling layer (Phase 2+)
class Logger;

struct EffectContext {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    uint32_t deviceFlags = 0;

    IFaceTracker* faceTracker = nullptr;
    ITexturePool* texturePool = nullptr;
    FrameScheduler* scheduler = nullptr;
    Logger* logger = nullptr;
};

} // namespace haocam
