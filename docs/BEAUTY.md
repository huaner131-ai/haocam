# Beauty (Facebetter) - Phase 2

## Contract

`IBeautyProvider` (extends `IEffectProvider`) exposes normalized setters:
`setSmoothing / setWhitening / setRosy / setSharpen / setFaceSlim /
setEyeSize / setNoseSize / setJawSlim`, plus `reset()`, `config()`, and
`process(Frame, FaceData)`. The UI talks only to the provider interface
(through `EffectManager`) - never to the SDK.

`FacebetterProvider` (src/effects/beauty/FacebetterProvider.cpp) is the
only translation unit allowed to include Facebetter headers; it is
compiled only when `HAOCAM_ENABLE_FACEBETTER=ON` **and** the SDK drop-in
exists under `sdk/facebetter/`. Otherwise `NullBeautyProvider` reports the
honest reason ("not configured" / "SDK not compiled in").

## Parameter model (spec sections 18/30)

* Application values are normalized `0.0` (off) .. `1.0` (maximum).
* Every public setter clamps to `[0,1]` (tested).
* The provider converts to SDK semantics:

| HaoCam slider | Facebetter 2.0 API | SDK range |
|---|---|---|
| Smoothing | `engine->SetSmoothing(v)` | [0,1] direct |
| Whitening | `engine->SetWhitening(v)` | [0,1] direct |
| Rosy | `engine->SetRosiness(v)` | [0,1] direct |
| Sharpen | `engine->SetSharpening(v)` | [0,1] direct |
| Face Slim | `engine->SetReshape(Reshape::FaceThin, v)` | [-1,1], app [0,1] mapped 1:1 |
| Eye Size | `engine->SetReshape(Reshape::EyeSize, v)` | [-1,1], app [0,1] mapped 1:1 |
| Nose | `engine->SetReshape(Reshape::NoseSlim, v)` | [-1,1], app [0,1] mapped 1:1 |
| Jaw | `engine->SetReshape(Reshape::Jawbone, v)` | [-1,1], app [0,1] mapped 1:1 |

Decision (documented, not silent): reshape values are passed as `[0,1]`
into the SDK's `[-1,1]` range - `0` is neutral in both; HaoCam does not
expose the negative (inverse) half in Phase 2.

## SDK 2.0 API used (verified against docs.facebetter.net + demo code)

```
SetLogConfig(LogConfig{console_enabled=false, ...})
engine = BeautyEffectEngine::Create(EngineConfig{
    app_id, app_key, license_token (optional, takes priority),
    resource_path = "sdk/facebetter/resource/resource.fbd",
    external_context = false})
engine->SetCallbacks(EngineCallbacks{
    on_engine_event,       // 0=license OK, 1=license failed,
                           // 100=init complete, 101=init failed
    on_face_landmarks})    // face count for diagnostics
engine->SetSmoothing / SetWhitening / SetRosiness / SetSharpening
engine->SetReshape(Reshape::..., v)
frame = ImageFrame::CreateWithRGBA(data, w, h, stride); frame->type = Video;
engine->ProcessImage(frame);  // RGBA in, output{Data,Width,Height,Stride} out
```

There is **no** type-enable step in SDK 2.0: an intensity > 0 enables the
effect. HaoCam re-applies only changed parameters (config diff per frame).

## Threading (spec sections 31/32)

* Setters are UI-thread safe: they swap an atomic config snapshot.
* The engine is created on a dedicated beauty worker thread; `initialize()`
  never blocks the caller (10 s retry loop while enabled).
* Per frame the worker: readback (GPU->CPU, reused buffers) -> RGBA
  conversion (reused buffers) -> `ProcessImage` -> upload result into a
  pooled BGRA texture -> publish to the output slot.
* The compositor consumes the beauty output only when it is fresh
  (`outputFrameId + 3 >= frameId`); otherwise the un-beautified frame is
  shown (brief last-state hold, never a frozen frame).

## Provider status (spec section 27)

`statusText()` reports (never logging credentials):
`Ready` / `Unavailable: ...` / `Invalid credentials (license event 1)` /
`Initialization failed (event 101)`. The QML Beauty panel shows the state
as a colored pill + detail text and disables sliders when unavailable.

## Honest status of this implementation

The adapter was written against the **official SDK 2.0 documentation and
demo code**, but the sandbox has **no Facebetter SDK and no credentials**,
so the engine was never initialized end-to-end. Do not assume runtime
behavior until verified on Windows hardware with a real SDK drop-in.
