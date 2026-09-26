# Beauty (Facebetter) - Phase 2

## Contract (spec section 10)

`IBeautyProvider` exposes: smoothing, whitening, rosy, sharpen, face slim,
eye size, nose size, jaw slim, `reset()`, and `process(Frame, FaceData)`.
`FacebetterProvider` is the only place Facebetter code may appear.

## SDK research status (verified 2026-09, see docs/SDK_INTEGRATION.md)

* Vendor: pixpark / facebetter.net. Desktop C++ SDK for Windows/Linux/macOS
  exists (repo: github.com/pixpark/facebetter-sdk; C++ API shown on the
  official site: `BeautyEffectEngine::Create(config)`,
  `SetBeautyTypeEnabled(BeautyType::Basic, true)`,
  `SetBeautyParam(beauty_params::Basic::Smoothing, 0.8f)`, `ProcessImage`).
* Authentication: `app_id` + `app_key` (developer account). HaoCam will ask
  the user for these via Settings and store them in the local settings file
  only - never hardcoded, never committed (spec section 5).
* GPU pipeline, realtime video mode (`ProcessMode::Video`).

## Phase-2 integration plan

1. Drop the SDK into `sdk/facebetter/` (gitignored) and build with
   `-DHAOCAM_ENABLE_FACEBETTER=ON`.
2. `FacebetterProvider::initialize` wraps the D3D11 device/input formats
   from `EffectContext`; `process()` stays GPU-resident end-to-end.
3. Face reshape (slim/eyes/nose/jaw) uses the shared `FaceData` landmarks.
4. Every parameter maps 1:1 to the Beauty tab sliders; `reset()` restores
   defaults. Availability and license errors surface in Settings
   (non-intrusive status, spec section 21).

Until Phase 2 the Beauty panel shows provider status and the app remains
fully functional without it.
