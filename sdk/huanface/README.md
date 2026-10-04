# HuanFace SDK (vendored)

This directory now CONTAINS the HuanFace SDK (`github.com/hanlimzie-glitch/HuanFace`,
`sdk/` subtree), vendored into HaoCam and patched:

- `patches/huanface/0001-wire-beauty-engine-into-HF_ProcessFrame.patch` is
  ALREADY APPLIED to `sdk/src/core/c_api.cpp` (the stock Phase-4 "minimal"
  pipeline left beauty as a pass-through and its with-face path returned a
  metadata-only output frame with no data).

Additional local SDK improvements (also shipped as
`patches/huanface/0002-perf-parallel-blur-and-mask-feather.patch` for
upstream): the CPU bilateral smoothing blur is row-parallelized (~10x on
multi-core desktops, was ~380 ms/frame single-threaded at 1280x720) and the
face/skin mask feather is widened 2px -> 12px so approximate landmarks no
longer produce a visibly hard effect boundary.

No drop-in, no copying, no `git apply` needed anymore: configure with
`-DHAOCAM_ENABLE_HUANFACE=ON`, set `"huanface": {"enabled": true}` in
`%LOCALAPPDATA%\HaoCam\config.json`, build, run.

The patch file is kept under `patches/huanface/` so it can be contributed
upstream to the HuanFace repository.
