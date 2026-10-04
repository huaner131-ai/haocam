# HuanFace SDK drop-in (optional)

[HuanFace](https://github.com/hanlimzie-glitch/HuanFace) is the user's own
beauty/face SDK. HaoCam integrates it through the PUBLIC C ABI
(`huanface_c_api.h`) only - never internal SDK headers.

## Install

```bash
git clone https://github.com/hanlimzie-glitch/HuanFace.git
cp -r HuanFace/sdk haocam/sdk/huanface/sdk
```

Expected layout:

```
sdk/huanface/sdk/
├── include/huanface_c_api.h   (+ huanface.hpp, huanface/)
├── src/                       (core, face, beauty, makeup, ...)
└── third_party/miniz/
```

## Apply the beauty-wiring patch

As of HuanFace b3139f2 the C-API pipeline runs tracking + makeup but leaves
BEAUTY as a pass-through ("no effect for Phase 4 minimal"). HaoCam needs the
beauty engine actually invoked; apply:

```bash
cd <your HuanFace checkout>
git apply haocam/patches/huanface/0001-wire-beauty-engine-into-HF_ProcessFrame.patch
```

(The patch maps the generic `beauty.*` float parameters onto the implemented
Phase-7 `FullBeautyEngine::ProcessCPU` and degrades to pass-through on
failure. PR it upstream so the fix lands in HuanFace itself.)

## Build

```powershell
cmake --preset windows -DHAOCAM_ENABLE_HUANFACE=ON
cmake --build --preset windows --config Release
```

## Configure

`%LOCALAPPDATA%\HaoCam\config.json`:

```json
{ "huanface": { "enabled": true } }
```

No credentials needed. Without ONNX models the SDK reports its honest
"heuristic fallback" tracking in the Beauty status line.
