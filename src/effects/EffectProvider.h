#pragma once

// Common provider abstraction (architecture spec, section 9).
//
// Every beauty / makeup / AR / native effect engine is wrapped behind this
// interface. The UI never talks to an external SDK directly; it talks to
// EffectManager, which owns providers.

#include <string>

namespace haocam {

enum class ProviderType {
    Beauty,
    Makeup,
    AR,
    Native,
};

const char* toString(ProviderType type);

class IEffectProvider {
public:
    virtual ~IEffectProvider() = default;

    // Initializes the provider with the shared effect context. A provider
    // must return false (and log the reason) when its SDK is unavailable;
    // this must never take the host application down.
    virtual bool initialize(const struct EffectContext& context) = 0;

    virtual void shutdown() = 0;

    // False when the SDK is missing, unlicensed or failed to initialize.
    virtual bool isAvailable() const = 0;

    virtual const char* name() const = 0;
    virtual ProviderType type() const = 0;

    // Human-readable availability status for the Settings UI, e.g.
    // "Unavailable: SDK not installed" or "Ready".
    virtual std::string statusText() const { return isAvailable() ? "Ready" : "Unavailable"; }
};

} // namespace haocam
