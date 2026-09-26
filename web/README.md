# Web Runtime (Phases 3-4)

HaoCam's bridge between the native C++ pipeline and web SDKs
(OpenMakeupSDK in Phase 3, Snap Camera Kit in Phase 4).

Planned layout:
  runtime/   index.html + bridge.js + runtime.js (structured JSON messaging)
  makeup/    OpenMakeup integration package (Phase 3)
  ar/        Snap Camera Kit web integration (Phase 4)

Nothing is faked: the runtime ships when the SDK integrations land
(docs/MAKEUP.md, docs/AR.md, docs/SDK_INTEGRATION.md).
