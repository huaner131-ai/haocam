# Effect System

## Contracts (implemented in Phase 1)

* `IEffectProvider` (`src/effects/EffectProvider.h`) - common provider
  interface with `initialize/shutdown/isAvailable/name/type` exactly as
  specified. `ProviderType` = Beauty | Makeup | AR | Native.
* `EffectContext` (`src/effects/EffectContext.h`) - D3D11 device/context +
  shared services (face tracker, texture pool, scheduler, logger).
* `EffectGraph` / `EffectNode` - the canonical 10-stage order from the spec
  as data. Stage enable flags are how availability is expressed.
* `EffectManager` - the only door between the UI and providers. Owns the
  compositor, exposes `providerStatus()` for the Settings panel, and reports
  every slot honestly:

  | Slot   | Provider        | Phase-1 status                                  |
  |--------|-----------------|-------------------------------------------------|
  | Beauty | Facebetter      | Unavailable - SDK not integrated yet (Phase 2)  |
  | Makeup | OpenMakeupSDK   | Unavailable - SDK not integrated yet (Phase 3)  |
  | AR     | Snap Camera Kit | Unavailable - SDK not integrated yet (Phase 4)  |
  | Native | HaoCam GPU      | Active (color conversion + composite)           |

## Failure handling (spec section 21)

Providers return `false` from `initialize()` when their SDK is missing,
unlicensed, or crashes-safe to skip; `EffectManager` logs it and disables the
stage. Camera preview and the remaining stages continue. Nothing in HaoCam
crashes because an SDK is absent.

## Phase roadmap

* Phase 2: `FacebetterProvider` implements `IBeautyProvider`
  (smoothing/whitening/rosy/sharpen/reshape) and the `FaceReshape` +
  `Beauty` stages come online; `FaceData` flows from the shared tracker.
* Phase 3: `OpenMakeupProvider` implements `IMakeupProvider` over the web
  runtime bridge (structured JSON messages, see docs/MAKEUP.md).
* Phase 4: `SnapCameraProvider` implements `IARProvider` over the web
  runtime bridge (see docs/AR.md).
* `LUTProvider`, `BackgroundProvider`, `StickerProvider` (native) arrive with
  the filters/background phases.
