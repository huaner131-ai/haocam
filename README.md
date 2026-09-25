# HaoCam — beauty & AR camera di browser

Kamera real-time bergaya **Prism Labs / Snow / Ulike / Snap Camera / TikTok Camera**:
face-mesh + GPU beauty pipeline + lens AR + color grading + rekam video.
Semua jalan on-device (WebGL2 + WebAssembly-free), tanpa server, tanpa CDN.

```
video kamera ──► fit/crop ──► MESH WARP ──► SKIN ──► EYES/TEETH ──► MAKEUP ──► BLOOM ──► GRADE ──► AR overlay ──► canvas
                    │            ▲             ▲          ▲             ▲                                   │
                    │        Delaunay     pyramid     masks (3×RGBA raster dari 68 landmark)          Canvas2D
                    └── face tracker (iBUG-68, face-api.js, lokal) ───────────────────────────────────────┘
```

## Kenapa framework-nya seperti itu

App beauty kamera komersial memakai arsitektur yang sama:

| Lapisan | Prism / Snap / TikTok | HaoCam (repo ini) |
| --- | --- | --- |
| App shell | Swift / Kotlin | **React 18 + TypeScript** (Vite) |
| Render | Metal / Vulkan / GLES, render-graph kecil | **WebGL2, render-graph manual** (`src/gl/gfx.ts`, `src/fx/pipeline.ts`) |
| Face | face mesh proprietary (ratusan titik) | **iBUG-68 landmark + Delaunay triangulation** → mesh warp |
| Skin | bilateral / surface-blur + skin segmentation | **pyramid bilateral + skin mask YCbCr** (`FS_SKIN`) |
| Reshape | MLS / mesh warp | **forward-warp segitiga Delaunay** (`src/face/mesh.ts`) |
| Grading | LUT 3D + curves | **LUT 3D (`.cube` import) + kurva analitik** (`FS_GRADE`, `src/fx/lut.ts`) |
| Lens | efek 2D/3D ter-anchor ke mesh | **Canvas2D prosedural ter-anchor ke mesh** (`src/fx/overlay.ts`) |
| Rekam | MediaCodec / AVFoundation | **MediaRecorder + canvas.captureStream** (`src/fx/capture.ts`) |

Dua hal yang sengaja dibuat "sama persis" secara perilaku:

1. **Pass berjenjang di GPU, bukan filter CSS.** Tiap efek = satu fragment shader yang menulis ke FBO RGBA8, ping-pong. Tidak ada `filter:` / `<canvas> 2d` di jalur utama, jadi tetap 60fps di HP.
2. **Semua efek dibimbing geometri wajah**, bukan "blur seluruh gambar". Mask (kulit/mata/bibir/gigi/undereye/alis/lipstik/blush/contour/highlight) di-raster dari 68 landmark tiap frame, lalu di-blur → feather.

## Menjalankan

```bash
npm install
npm run dev        # http://localhost:5173  (dev server bind 0.0.0.0)
npm run build      # typecheck + produksi ke dist/
npm test           # uji logika murni (geometri, mesh warp, mask, LUT) — tanpa GPU
```

Buka di browser → izinkan kamera. Atau lempar (drag & drop) file video/foto ke stage untuk memproses material tanpa kamera, atau **Rekam layar** sebagai sumber.

> Butuh HTTPS (atau localhost) supaya `getUserMedia` boleh dipakai.

## Fitur

**Beauty** — 8 preset (Raw, Auto, Korean Milk, Prism Soft, Porcelain, Cinema Skin, Groomed) + master intensity, dengan slider granular:
smoothing (surface/airbrush, scale), retain detail, even tone, blemish fix, brighten, matte/de-shine, glow.

**Bentuk wajah (mesh warp)** — V-line/rahang, cheekbone, panjang dagu, tinggi jidat, ukuran & jarak mata, slimmer/panjang hidung, lip plump, auto-smile (dipicu ekspresi), brow lift, face taper. Ada guard: saat wajah menoleh > ±45° displacement otomatis diurangi, dan anchor di luar kotak wajah tidak digeser sama sekali (background tidak "melar").

**Mata & gigi** — sclera whitening, eye bright + unsharp lokal, iris pop, dark-circle/eye-bag, lashes & eyeliner yang digambar dari spline kelopak, catchlight, teeth whitening + polish.

**Makeup** — blush, lip tint + gloss, eyeshadow di kelopak, brow fill, highlighter (tulang pipi / jembatan hidung / cupid's bow), contour (siluet rahang, pelipis, sisi hidung), freckle, glass-skin veil, palet warna + color picker.

**Filter / grading** — 12 preset (Golden Hour, Peach Soda, Cold Brew, Matcha, Sakura, Noir, Retro DV, Y2K Chrome, Milk Tea, Kodak 400, Bleach, Original) + 17 slider manual (exposure, temp, tint, highlights/shadows, fade, grain, vignette, bloom, halation, soft focus, clarity, sharpen, ring light) + **import file `.cube`** (LUT 3D, sampling LINEAR).

**Lens AR** — Cat, Bunny, Halo, Crown, Heart Eyes, Shades, Glitter, Star Tears, Whiskers, Mecha Scan, Bubblegum; ter-anchor ke garis rambut/pusat mata/ujung hidung, ikut roll & skala wajah, bereaksi ke senyum dan buka mulut.

**Frame** — Instant (polaroid), Film Strip, Shot on HaoCam, Vignette, Story UI.

**Kamera** — pilih device, tukar depan/belakang, mirroring, rasio 9:16 / 4:5 / 1:1 / 16:9 / full, zoom (wheel/pinch), rotate 90°, tahan canvas = before/after split, foto PNG, rekam video (mp4/H.264 bila didukung, fallback webm VP9/VP8) + audio mic, bitrate & fps diatur, share sheet / clipboard.

**Pro** — semua slider terbuka, simpan muat look ke `localStorage`, export/import JSON, overlay debug face-mesh, quality (Hemat/Seimbang/Ultra → resolusi internal + laju tracker), akurasi tracker (68-tiny / 68-full), input size detektor.

## Tentang plugin OBS "Beauty Filter" (`.exe` yang kamu kirim)

Singkat: **isi installer `.exe` itu tidak bisa dipakai langsung, tapi hasilnya bisa — dan parameternya bisa.** Detail:

1. **File-nya tidak bisa diambil dari sandbox.** Saya coba: koneksi TLS ke `download1338.mediafire.com` ditolak jaringan sandbox (`SSL_ERROR_SYSCALL`), jadi unduhannya tidak pernah sampai. Domain yang lolos di sini hanya registry npm dan github.
2. **Kalaupun terunduh, itu bukan "data" yang bisa dipakai app.** Isinya installer NSIS Windows: `*.dll` filter OBS (gfx pipeline Direct3D/OpenGL + C++/OpenCV, format config khas OBS). Web/mobile camera app jalan di WebGL2/WASM — arsitekturnya tidak ketemu; menyalin biner `.dll` tidak ada artinya di browser, dan menyebarluaskan biner/model orang lain juga masalah lisensi.
3. **Yang saya lakukan invece:** meng-*reimplement* resep filter beauty yang sama (skin segmentation YCbCr → bilateral/pyramid smoothing → detail retention → mesh warp landmark → eye/teeth correction → LUT grading) memakai model yang bebas dipakai dan bisa di-vendor: `face-api.js` (detektor + landmark 68 titik + ekspresi) saya simpan di `public/vendor/` (~1 MB) sehingga app jalan **offline**, tanpa CDN Google/jsdelivr yang juga diblokir di sini.
4. **Jalur integrasi OBS yang benar-benar jalan (dan sudah saya siapkan):**
   - install plugin beauty di OBS (Windows) → tambahkan filter ke *Video Capture Device* / *Scene*;
   - klik **Start Virtual Camera** di OBS;
   - di HaoCam buka **⚙ → Kamera / device input → pilih `OBS Virtual Camera`**.
   HaoCam menganggap output OBS sebagai frame mentah, lalu beautify + grade + lens + rekam di GPU. Efek OBS (beauty, LUT, koreksi warna) tetap terlihat; slider HaoCam menumpuk di atasnya.
5. **Kalau maksudmu "pakai datanya" = pakai angka presetnya:** itu sangat bisa. Simpan scene OBS-mu, lalu kirim isi `profile/collections/*/scene.json` (atau `basic/scenes/*.json`) bagian `"filters"` → `"id": "...beauty..."` → `"settings": { ... }`. Semua nilai plugin (strength smoothing, whitening, sharpness, face slim, eye size, dll.) bisa saya petakan 1:1 ke `FxParams` HaoCam sehingga look-nya identik. Tempel JSON-nya, saya buatkan mapper + preset barunya.

Alternatif lain: kalau kamu punya **source code** plugin itu (banyak plugin beauty OBS open-source: C++/OpenCV), sebutkan repo-nya — algoritmanya (urutan pass, sigma filter, cara mask kulit, formula whitening) bisa saya port presisi ke GLSL di `src/gl/shaders.ts`.

## Struktur

```
src/
  gl/gfx.ts              WebGL2 context, Program/uniform, Target/FBO, fullscreen triangle
  gl/shaders.ts          semua body shader (input, down-bilateral, mesh, mask, skin, features, makeup, bloom, grade, present)
  face/landmarks.ts      indeks iBUG-68, region, pose (roll/yaw/pitch), spline, hull
  face/mesh.ts           awan anchor + Delaunay + displacement reshape + buildGeom
  face/tracker.ts        loader face-api.js (UMD lokal) + EMA smoothing landmark + ekspresi
  fx/pipeline.ts         render graph, fit-matrix, proyeksi landmark ke ruang output, upload tekstur
  fx/masks.ts            raster region → 3×RGBA (12 kanal) untuk memandu tiap efek
  fx/params.ts           FxParams + preset beauty/filter + lens/frame + kualitas
  fx/overlay.ts          lens AR + frame (Canvas2D prosedural) + mesh debug
  fx/lut.ts              parser .cube + generator LUT prosedural
  fx/capture.ts          MediaRecorder, download, share, clipboard
  engine.ts              sumber (kamera/layar/file), render loop, foto/rekam, status
  state.ts               store params (React external store) + localStorage
  ui/                    App shell (panels, modal sumber, controls)
public/vendor/           face-api.js + model .bin (vendored, offline)
test/logic.ts            assertion geometri/warp/mask/LUT (node, tanpa GPU)
```

## Batasan yang jujur

- Model deteksi = face-api (68 titik). Lebih hemat dari mesh 468 titik, tapi region halus (sisi hidung, kantong mata) diperkirakan lewat spline + feather, bukan semant segmentation. Bila mau, jalur tracker sudah saya buat bisa ditukar: tinggal tambah tracker MediaPipe 468 titik + blendshapes.
- Lens AR 2D ter-anchor (bukan mesh 3D bertekstur dengan oklusi); tidak menutupi objek di depan wajah.
- Rekam video = apa yang didukung `MediaRecorder` browser. MP4/H.264 dipilih kalau tersedia (Chrome/Edge/Safari), selain itu WebM.
- Deteksi wajah berjalan asinkron; landmark di-EMA supaya warp tidak bergetar saat frame tracker lebih lambat dari render.
- Tidak ada upload ke server mana pun — semua lokal.

## Roadmap (ikut pola app sejenis)

- [ ] tracker 468 titik + blendshapes (kedip, buka mulut, lidah) untuk lens oklusi
- [ ] multi-face (mask per-ID) & face-retention saat tracking hilang
- [ ] video edit: timeline pendek + musik (WebAudio) + export bitrate
- [ ] skin tone-aware whitening (LCH) agar tidak abu-abu pada kulit sawo matang
- [ ] mode "auto" yang memilih strength dari kondisi cahaya (luma histogram dari texture readback sekali-sekali)
- [ ] PWA install + offline shell + shortcut tombol volume untuk shutter
