# SDK Integration Research

Findings as of **2026-09-26**, gathered from official documentation before
any integration work (spec section 37). Re-verify at integration time.

## Facebetter (Beauty - Phase 2)

| Question | Finding | Source |
|---|---|---|
| Supported platforms | iOS, Android, Web, Windows, macOS, Linux (desktop C/C++ API) | facebetter.net, github.com/pixpark/facebetter-sdk |
| Native C++ on Windows | Yes (desktop C++ sample: unzip SDK into `demo/cpp/sdk/`, CMake build) | github.com/pixpark/facebetter-sdk |
| License/auth | Developer account; `app_id` + `app_key` configured per app | facebetter.net docs (EngineConfig) |
| GPU texture sharing | SDK advertises GPU rendering pipeline & realtime video processing; exact interop formats to verify at integration (D3D11 texture input vs internal upload) | facebetter.net product overview |
| WebView integration | Not required on desktop | - |
| Key APIs | `BeautyEffectEngine::Create`, `SetBeautyTypeEnabled`, `SetBeautyParam(Smoothing/Whitening/...)`, `ProcessImage(frame)` | facebetter.net homepage sample |
| Redistribution | SDK download behind the vendor site; license terms to review before shipping binaries | facebetter.net/download |
| Action for HaoCam | Config mechanism for app_id/key (Settings, local-only storage). No credentials in repo. | spec section 5 |

## OpenMakeupSDK (Makeup - Phase 3)

| Question | Finding |
|---|---|
| Official distribution | Not found as of 2026-09 (only third-party references). Treat as web/WebGL per the product brief. |
| Native C++ | No evidence - do NOT pretend it is native (spec section 3). |
| Integration path | Web Runtime (WebView2) bridge once official access exists. |
| Status in HaoCam | Adapter interface shipped; provider reports Unavailable; no fake integration. |

## Snap Camera Kit (AR - Phase 4)

| Question | Finding | Source |
|---|---|---|
| Supported platforms | iOS, Android, **Web**. No native Windows/macOS SDK. | developers.snap.com/camera-kit/home |
| Desktop story | Snap Camera (desktop app) shut down 2023-01; no official replacement. | ar.snap.com blog + press |
| Access/licensing | Application/approval process (ar.snap.com/camera-kit); API token per app; usage limits and ToS apply. | official Camera Kit docs |
| Web SDK | Camera Kit Web SDK (JS) intended for web apps - the sanctioned path for HaoCam's WebView2 runtime. | developers.snap.com/camera-kit/web |
| Input/Output | Frames into the SDK session, AR output composited by the SDK; lens groups configured in the developer portal. | official docs |
| Key risks | Token security (local storage only), lens group curation, ToS acceptance by the app publisher. | - |
| Action for HaoCam | Configuration mechanism for the API token; honest Unavailable state without it; never bundle or extract Snap assets. | spec sections 4/5/36 |

## Cross-cutting rules (spec section 5)

* Official SDK distributions and documented APIs only.
* No license/DRM/watermark/limit bypassing. No asset extraction from
  third-party apps. No reverse engineering of protections.
* User-provided credentials live in the local settings file
  (`%LOCALAPPDATA%/HaoCam/settings.json`); `sdk/` contents are gitignored.
* Every SDK slot has a functional placeholder that reports Unavailable so
  the rest of HaoCam keeps working.
