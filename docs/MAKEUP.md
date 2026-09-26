# Makeup (OpenMakeupSDK) - Phase 3

## Contract (spec section 11)

`IMakeupProvider` exposes foundation, blush, lipstick, eyeliner, mascara,
eyeshadow as `MakeupLayer` values (enable/color/opacity/finish/pattern),
`clearAll()`, and `process(Frame, FaceData)`.

## Research status (2026-09)

* No official public distribution of "OpenMakeupSDK" could be located as of
  this phase (searches surface only third-party projects referencing the
  name). The master brief states it is web/WebGL based.
* HaoCam therefore treats it as a **web-SDK integration through the Web
  Runtime bridge** (`web/runtime`, WebView2) once official access is
  available. Until then the provider reports Unavailable (honest, no fake
  integration - spec sections 35/36).
* Integration is blocked on obtaining the SDK from its maintainer; the
  adapter interface and bridge are already in place so the provider can be
  added without touching the core.

## Bridge message format (spec section 20)

```json
{
  "type": "makeup.set",
  "category": "lipstick",
  "enabled": true,
  "color": "#C73563",
  "opacity": 0.75
}
```

Responses arrive as structured status events; failures disable the makeup
stage and surface in Settings. No JavaScript is embedded in the C++ core -
the bridge owns all web-side code.

## Phase-3 order

Foundation, blush, lipstick first; then eyeshadow, eyeliner, mascara.
Placement uses the shared `FaceData` landmarks (68-point canonical indices
in `src/face/FaceLandmarks.h`) forwarded to the web runtime with each frame
batch.
