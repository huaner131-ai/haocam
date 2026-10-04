# HuanFace SDK (vendored)

This directory now CONTAINS the HuanFace SDK (`github.com/hanlimzie-glitch/HuanFace`,
`sdk/` subtree), vendored into HaoCam and patched:

- `patches/huanface/0001-wire-beauty-engine-into-HF_ProcessFrame.patch` is
  ALREADY APPLIED to `sdk/src/core/c_api.cpp` (the stock Phase-4 "minimal"
  pipeline left beauty as a pass-through and its with-face path returned a
  metadata-only output frame with no data).

No drop-in, no copying, no `git apply` needed anymore: configure with
`-DHAOCAM_ENABLE_HUANFACE=ON`, set `"huanface": {"enabled": true}` in
`%LOCALAPPDATA%\HaoCam\config.json`, build, run.

The patch file is kept under `patches/huanface/` so it can be contributed
upstream to the HuanFace repository.
