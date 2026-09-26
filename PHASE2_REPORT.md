# HaoCam — Laporan Phase 2 (Face Tracking + Facebetter Beauty)

**Commit:** [`d0679b7`](https://github.com/huaner131-ai/haocam/commit/d0679b7) pada branch `arena/01a0de8b-haocam` (base: `6d0be41`, Phase 1 tidak diubah ulang).

---

## 1. Files (57 file, +4.655/−314)

| Area | File baru | File yang diubah |
|---|---|---|
| Face tracking | `src/face/FaceLandmarks.{h,cpp}`, `FaceTracker.h`, `FaceTracking.cpp`, `LandmarkSmoother.h`, `TrackingWorker.{h,cpp}`, `MediaPipeFaceTracker.{h,cpp}`, `MediaPipePixelSource.{h,cpp}` | `FaceData.h` |
| Beauty | `src/effects/beauty/BeautyProvider.h`, `BeautyConfig.cpp`, `NullBeautyProvider.{h,cpp}`, `FacebetterProvider.{h,cpp}` | — |
| Engine/inti | `src/core/config/AppConfig.{h,cpp}`, `src/graphics/D3D11/GpuFrameCopier.{h,cpp}`, `src/graphics/shaders/tracker_ps.hlsl` | `Compositor.{h,cpp}`, `EffectManager.{h,cpp}`, `EffectManager.cpp` |
| UI | `ui/controllers/BeautyController.{h,cpp}` | `EngineController.{h,cpp}` (engine thread), `DiagnosticsController.{h,cpp}`, `VideoView.cpp`, `BeautyPanel.qml`, `CameraView.qml`, `app/App.{h,cpp}` |
| Build/aset | `config.example.json`, `assets/models/README.md`, `sdk/mediapipe/README.md`, `sdk/facebetter/README.md` | `CMakeLists.txt`, `tests/CMakeLists.txt`, `.gitignore`, `docs/BUILD.md` |
| Test & docs | 5 file test baru | `docs/FACE_TRACKING.md`, `BEAUTY.md`, `GPU_PIPELINE.md`, `SDK_INTEGRATION.md`, `ARCHITECTURE.md`; `WINDOWS_VERIFICATION.md` (baru) |

## 2. Face Tracking

* **Kontrak independen provider**: `IFaceTracker` + factory `createDefaultTracker()` — MediaPipe **hanya** di dalam adapter `MediaPipeFaceTracker.cpp` (CMake-gated `HAOCAM_HAS_MEDIAPIPE`); tanpa SDK → `NullFaceTracker` yang jujur melapor `Unavailable` (pipeline tetap jalan, tidak pernah crash).
* **FaceData**: landmark ternormalisasi `[0,1]`, bounds kotak ketat, confidence = rata-rata presence, pose (konvensi terdokumentasi di `FaceData.h`: yaw+ = wajah menoleh ke *kiri subjek* / kanan penonton; pitch+ = menengadah; roll+ = puncak kepala miring ke kanan penonton; satuan derajat). Akses semantik via enum `FaceLandmark` + `getLandmark()` — **tidak ada indeks landmark MediaPipe di luar adapter** (semua konstanta `kMp*` hanya di `FaceLandmarks.h`).
* **Pipeline**: `WEBCAM → Media Foundation → D3D11 → engine thread → TrackingWorker (latest-slot, drop-stale, throttle 30 fps) → FaceData → provider`. Tracking **tidak pernah di UI thread**; kamera tidak pernah menunggu tracker.
* **Path piksel GPU-first**: downscale GPU ≤480 px (shader `tracker_ps.hlsl` mengonversi BGRA→RGBA), readback staging ring 2 slot (buffer dipakai ulang, tanpa alokasi per frame, tanpa download tekstur penuh). Fallback CPU NV12→RGBA untuk non-Windows/test.
* **Smoothing temporal** (One Euro adaptif, terdokumentasi): `cutoff = minCutoff + β·|speed|`, `α = 1/(1 + 1/(2π·cutoff·dt))`; gap >0,5 s → pass-through (tidak ada smear). Teruji: adaptivitas, gap, ganti layout (468↔68), kestabilan.
* **Model**: `face_landmarker.task` **tidak pernah di-download runtime, tidak di-commit**; model hilang → tracker `Unavailable` (lihat `assets/models/README.md`).

## 3. Facebetter

* `FacebetterProvider` ditulis terhadap **dokumentasi resmi SDK 2.0** (docs.facebetter.net quick-start + implement-beauty, diverifikasi silang dengan demo/cpp di repo pixpark/facebetter-sdk): `SetLogConfig → Create(EngineConfig{app_id, app_key, license_token?, resource_path=resource.fbd}) → SetCallbacks(event 0/1/100/101) → SetSmoothing/SetWhitening/SetRosiness/SetSharpening + SetReshape(FaceThin/EyeSize/NoseSlim/Jawbone) → CreateWithRGBA/ProcessImage`. Tidak ada nama API yang dikarang; tidak ada langkah "type-enable" (SDK 2.0: intensitas >0 = aktif).
* **Kredensial** hanya lewat `%LOCALAPPDATA%\HaoCam\config.json` (contoh: `config.example.json`); tidak pernah di-commit, tidak pernah di-log (log hanya "credentials=present/missing").
* **Threading**: engine dibuat di beauty worker thread (async init, retry 10 s, UI tidak pernah blocking saat slider digeser — setter = snapshot atomik + config-diff per frame); readback→RGBA (buffer reuse)→ProcessImage→upload BGRA pool→slot output; komposit memakai output hanya jika segar (`frameId+3 ≥ frame.id` — tanpa mask beku).
* **Status runtime**: `Ready / Unavailable: ... / Invalid credentials (event 1) / Initialization failed (event 101)` via `statusText()` + event bus; gagal → beauty off, kamera jalan terus.

## 4. Beauty (UI)

* Tab Beauty berfungsi: **Skin** (Smoothing, Whitening, Rosy, Sharpen) + **Face** (Face Slim, Eye Size, Nose, Jaw), slider 0–100 % (ternormalisasi 0–1; provider yang mengonversi ke rentang SDK — mapping reshape [0,1]→[-1,1] didokumentasikan, tanpa pembalikan tanda), tombol **Reset**, pil status berwarna + pesan "Facebetter unavailable" (§26/27), slider nonaktif saat provider tidak tersedia.
* Semua parameter di-clamp `[0,1]` (diuji), persist ke settings (debounced).

## 5. Windows tested? — **TIDAK (jujur)**

Tidak ada perangkat keras Windows / VM / GPU di lingkungan pengembangan Phase 2 — verifikasi runtime **tidak dilakukan** dan **tidak ada klaim runtime**. Yang dilakukan sebagai gantinya:

* 4 TU native Windows (`MediaPipePixelSource.cpp`, `GpuFrameCopier.cpp`, `Compositor.cpp`, `EffectManager.cpp`) lolos *object-compile* silang `zig c++ -target x86_64-windows-gnu` (menemukan & memperbaiki 2 bug nyata: argumen `CreateTexture2D`, include `EffectContext.h`).
* Detail lengkap + checklist first-run ada di `WINDOWS_VERIFICATION.md`. Qt/QML/moc **tidak pernah dikompilasi** di sandbox (Phase 1 maupun 2) — tetap belum terverifikasi kompilasi.
* Facebetter **tidak pernah diinisialisasi** (tidak ada SDK/kredensial) — keberhasilannya tidak bisa diklaim; MediaPipe **tidak pernah dikompilasi/dijalankan** (tidak ada SDK drop-in/model).

## 6. Linux tests — **62/62 lulus**

`linux-core`: configure + build bersih (GNU 12.2), ctest 1/1. **35 test baru** (total 62, termasuk 27 test Phase 1 yang tetap hijau): normalisasi/bounds/konvensi pose FaceData, mapping landmark 68, akses semantik NaN-safe, smoother (adaptif/gap/layout), NullFaceTracker + pemilihan `primary()`, default config tracker, BeautyConfig default/clamp/JSON, NullBeautyProvider (alasan jujur, clamp, reset), parsing AppConfig + kredensial, urutan graph tracking→beauty→composite.

## 7. Performance (terukur, Linux `-O2`)

| Pengukuran | Nilai |
|---|---|
| `LandmarkSmoother::apply` (68 titik) | **519 ns/call** (~1,6 % dari budget frame 30 fps) |
| `estimateHeadPose` (geometris, 68) | **32 ns/call** |
| Readback tracker (perkiraan desain, belum diukur runtime) | ≤ ~0,9 MB per frame terlacak @480 px RGBA — **belum diukur di Windows** |

Tidak ada klaim FPS runtime (tidak ada kamera/GPU di sandbox).

## 8. Known Issues / Batasan

1. Qt/QML + controller baru (`BeautyController`, `EngineController` baru, `DiagnosticsController`) **belum pernah dikompilasi** (tidak ada Qt di sandbox) — review manual API sudah dilakukan, kompilasi pertama menunggu Windows.
2. `FacebetterProvider.cpp` & `MediaPipeFaceTracker.cpp` hanya ter-compile ketika SDK drop-in ada; keduanya diverifikasi terhadap dokumen/header resmi, bukan terhadap kompilasi SDK sungguhan.
3. Konvensi pose dari matriks transformasi MediaPipe perlu divalidasi ulang saat runtime (tanda diekstrak sesuai konvensi terdokumentasi; fallback geometris sudah diuji).
4. `PerfParser` (`Json.cpp`) masih menyisakan warning kosmetik warisan Phase 1 (tidak berpengaruh).

## 9. Next — Phase 3 (rencana, belum dikerjakan)

Integrasi **Makeup via OpenMakeupSDK** (adapter sudah disiapkan sebagai slot `Unavailable` di provider status; path web runtime sesuai riset Phase 2), termasuk konsumsi `FaceData` untuk penempatan layer makeup.
