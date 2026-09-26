# AR (Snap Camera Kit) - Phase 4

## Contract (spec section 12)

`IARProvider` exposes `availableEffects()`, `loadEffect(id)`,
`unloadEffect()`, `setParameter(name, value)`, `process(Frame, FaceData)`.
`SnapCameraProvider` is the only place Snap code may appear.

## Research status (verified 2026-09 against official docs)

* Snap's official documentation (developers.snap.com/camera-kit) lists
  Camera Kit SDKs for **iOS, Android and Web only**. There is **no native
  Windows C++ SDK**. Snap Camera (the old desktop app) was shut down in
  January 2023.
* Access requires an approved application (ar.snap.com/camera-kit) and an
  API token from the Snap Kit developer portal. Licensing/usage terms must
  be accepted by the app developer - HaoCam will never bypass them
  (spec section 5).
* Therefore Phase 4 integrates the **Camera Kit Web SDK inside HaoCam's Web
  Runtime (WebView2)**, feeding frames to the web side and compositing the
  AR output back into the GPU pipeline. If a native SDK appears later, the
  adapter interface stays identical.

## Honest fallback (spec sections 35/36)

`SnapCameraProvider::isAvailable()` checks for (a) a configured API token in
Settings, and (b) the web runtime being loadable. Without either, AR reports
"Unavailable" and beauty/makeup/preview continue unaffected:

```
[WARN] [ar        ] Snap Camera Kit unavailable: no API token configured
```

No Snap code, assets or lens archives are bundled or extracted from other
applications.

## Bridge message format (spec section 20)

```json
{ "type": "ar.load", "effectId": "cat_ears" }
{ "type": "ar.status", "success": true }
```

## Phase-4 UI

Lens browser with search, categories, favorites, recently used, per-lens
parameters - all backed by `availableEffects()` metadata from the configured
lens group.
