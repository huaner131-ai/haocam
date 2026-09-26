#pragma once

// The single entry point between the UI and effect providers
// (architecture spec, section 16). The UI communicates ONLY with
// EffectManager - never with Facebetter / OpenMakeup / Snap directly.
//
// Phase 1 status:
//   * beauty() / makeup() / ar() return nullptr and the status registry
//     reports the provider as unavailable.
//   * process() executes the enabled graph stages that exist today
//     (color conversion + final composite run inside the GPU compositor,
//     which EffectManager drives).

#include <memory>
#include <vector>

#include "effects/EffectGraph.h"
#include "effects/EffectProvider.h"

namespace haocam {

class IBeautyProvider;
class IMakeupProvider;
class IARProvider;
struct EffectContext;
class Compositor;

class EffectManager {
public:
    EffectManager();
    ~EffectManager();

    EffectManager(const EffectManager&) = delete;
    EffectManager& operator=(const EffectManager&) = delete;

    // Creates the compositor and initializes the tracker + registered
    // providers. Missing SDKs are logged, never fatal.
    bool initialize(const EffectContext& context);
    void shutdown();

    // Provider accessors. Return nullptr when a provider is not registered
    // or not available (UI must handle this gracefully).
    IBeautyProvider* beauty();
    IMakeupProvider* makeup();
    IARProvider* ar();

    // Advances the pipeline for one frame: runs enabled nodes in order.
    // `frame` is updated in place (GPU textures are swapped between passes).
    void process(Frame& frame);

    // GPU compositor (Windows/D3D11 builds); nullptr elsewhere or when GPU
    // initialization failed. The preview reads final output from here.
    ::haocam::Compositor* compositor() const;

    void reset();

    EffectGraph& graph() { return m_graph; }
    const EffectGraph& graph() const { return m_graph; }

    // Status registry for the Settings panel: one entry per provider slot.
    struct ProviderStatus {
        std::string slot;      // "Beauty", "Makeup", "AR", "Native"
        std::string provider;  // engine name, e.g. "Facebetter"
        bool available = false;
        std::string detail;    // human-readable availability detail
    };
    std::vector<ProviderStatus> providerStatus() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    EffectGraph m_graph;
};

} // namespace haocam
