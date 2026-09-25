/**
 * Shader body untuk tiap pass pipeline.
 *
 * Konvensi: semua body di-`prefix` dengan `#version 300 es` + GLSL_COMMON
 * (lihat Gfx.link) — jadi tidak perlu menulis ulang helper di tiap shader.
 * Fragment shader menerima `in vec2 vUv` (y-down, 0..1) dan menulis `outColor`.
 */

/* ────────────────────────────────────────────────────────────── input */
export const FS_INPUT = /* glsl */ `
uniform sampler2D uTex;
uniform mat3 uFit;      // uv output → uv kamera
uniform float uSharpen; // pra-penajaman ringan utk video/screenshot yang di-upscale
in vec2 vUv;
out vec4 outColor;
void main() {
  vec2 p = (uFit * vec3(vUv, 1.0)).xy;
  if (p.x < -0.01 || p.x > 1.01 || p.y < -0.01 || p.y > 1.01) { outColor = vec4(0.015, 0.015, 0.02, 1.0); return; }
  vec3 c = texture(uTex, p).rgb;
  if (uSharpen > 0.0) {
    vec2 tx = 1.0 / vec2(textureSize(uTex, 0));
    vec3 blur = (texture(uTex, p + vec2(tx.x, 0.0)).rgb + texture(uTex, p - vec2(tx.x, 0.0)).rgb
               + texture(uTex, p + vec2(0.0, tx.y)).rgb + texture(uTex, p - vec2(0.0, tx.y)).rgb) * 0.25;
    c += (c - blur) * uSharpen;
  }
  outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}`;

/* ─────────────────────────────────────────── pyramid downblur (bilateral) */
export const FS_DOWNBLUR = /* glsl */ `
uniform sampler2D uTex;
uniform vec2 uSrcTexel;
uniform float uSigma;    // radius dlm pixel sumber
uniform float uRange;     // sigma luma (edge aware)
uniform float uWeight;    // 0 = gaussian murni, 1 = bilateral penuh
in vec2 vUv;
out vec4 outColor;
void main() {
  vec2 tx = uSrcTexel * max(uSigma, 0.6);
  vec3 c0 = texture(uTex, vUv).rgb;
  float l0 = luma(c0);
  vec3 sum = c0;
  float wsum = 1.0;
  // 13-tap spiral: murah, cukup halus utk pyramid beauty
  for (int i = 1; i <= 12; i++) {
    float fi = float(i);
    float ang = fi * 2.39996;
    float rad = sqrt(fi / 12.0);
    vec2 o = vec2(cos(ang), sin(ang)) * rad;
    vec3 s = texture(uTex, vUv + o * tx).rgb;
    float dl = abs(luma(s) - l0);
    float w = exp(-dl * dl / max(uRange * uRange, 1e-5));
    w = mix(1.0, w, uWeight);
    sum += s * w;
    wsum += w;
  }
  outColor = vec4(sum / wsum, 1.0);
}`;

/* ───────────────────────────────────────────────── mesh (warp) sampling */
export const VS_MESH = /* glsl */ `
layout(location = 0) in vec2 aPos;   // dest (normalized, y-down)
layout(location = 1) in vec2 aUv;    // src  (normalized, y-down)
out vec2 vUv;
void main() {
  vUv = aUv;
  gl_Position = vec4(aPos.x * 2.0 - 1.0, 1.0 - aPos.y * 2.0, 0.0, 1.0);
}`;

export const FS_WARP_SAMPLE = /* glsl */ `
uniform sampler2D uTex;
in vec2 vUv;
out vec4 outColor;
void main() { outColor = vec4(texture(uTex, vUv).rgb, 1.0); }`;

/* ───────────────────────────────────────────────── rasterisasi mask */
export const VS_MASK = /* glsl */ `
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
out vec4 vColor;
void main() {
  vColor = aColor;
  gl_Position = vec4(aPos.x * 2.0 - 1.0, 1.0 - aPos.y * 2.0, 0.0, 1.0);
}`;

export const FS_MASK = /* glsl */ `
in vec4 vColor;
out vec4 outColor;
void main() { outColor = vColor; }`;

export const FS_BLUR2 = /* glsl */ `
uniform sampler2D uTex;
uniform vec2 uTexel;
uniform vec2 uDir;      // (1,0) / (0,1)
uniform float uRadius;
uniform float uKeepAlpha; // 1 = blur RGB saja (bloom), 0 = blur 4 kanale (mask)
in vec2 vUv;
out vec4 outColor;
void main() {
  vec2 o = uDir * uTexel * max(uRadius, 0.0);
  vec4 c = texture(uTex, vUv) * 0.2270270270;
  c += (texture(uTex, vUv + o) + texture(uTex, vUv - o)) * 0.1945945946;
  c += (texture(uTex, vUv + o * 2.0) + texture(uTex, vUv - o * 2.0)) * 0.1216216216;
  c += (texture(uTex, vUv + o * 3.0) + texture(uTex, vUv - o * 3.0)) * 0.0540540541;
  c += (texture(uTex, vUv + o * 4.0) + texture(uTex, vUv - o * 4.0)) * 0.0162162162;
  c.a = mix(c.a, texture(uTex, vUv).a, uKeepAlpha);
  outColor = c;
}`;

/* helper bersama untuk membaca mask + region kulit bersih */
const MASK_SLOTS = /* glsl */ `
uniform sampler2D uMaskA; // r skin, g mata, b bibir, a gigi
uniform sampler2D uMaskB; // r undereye, g alis, b liner+lash, a hidung
uniform sampler2D uMaskC; // r blush, g kelopak, b highlight, a contour
vec4 mA(vec2 uv){ return texture(uMaskA, uv); }
vec4 mB(vec2 uv){ return texture(uMaskB, uv); }
vec4 mC(vec2 uv){ return texture(uMaskC, uv); }
float skinMask(vec2 uv){
  vec4 a = mA(uv), b = mB(uv);
  float holes = max(max(a.g, a.b), max(b.g * 0.85, b.b));
  return clamp(a.r * (1.0 - holes), 0.0, 1.0);
}`;

/* ────────────────────────────────────────────────────────── skin pass */
export const FS_SKIN = /* glsl */ `
${MASK_SLOTS}
uniform sampler2D uSrc;
uniform sampler2D uPyr0;
uniform sampler2D uPyr1;
uniform sampler2D uPyr2;
uniform vec2 uSrcTexel;
uniform float uSmooth, uTexture, uWhiten, uToneEven, uBlemish, uMatte, uGlow, uSmoothMode, uGlowAmt;
in vec2 vUv;
out vec4 outColor;

// YCbCr (BT.601) TANPA offset 0.5 → chroma berpusat di 0, round-trip eksak
// (luma yang dipakai untuk rekonstruksi harus luma BT.601 yang sama, kalau tidak
//  warna akan bergeser saat "even tone" menaikkan/bassa chroma)
vec2 toChroma(vec3 c) {
  return vec2(dot(c, vec3(-0.168736, -0.331264, 0.5)), dot(c, vec3(0.5, -0.418688, -0.081312)));
}
float luma601(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
vec3 fromChroma(float y, vec2 cbcr) {
  float cb = cbcr.x, cr = cbcr.y;
  return clamp(vec3(y + 1.402 * cr, y - 0.344136 * cb - 0.714136 * cr, y + 1.772 * cb), 0.0, 1.0);
}

void main() {
  vec3 c = texture(uSrc, vUv).rgb;
  float skin = skinMask(vUv);
  float skinLike = skin * (0.55 + 0.45 * skinness(c));

  // 2 level blur (hemispheric): pyr1 (¼) + pyr2 (⅛) → halus tapi tetap 3D
  vec3 b1 = texture(uPyr1, vUv).rgb;
  vec3 b2 = texture(uPyr2, vUv).rgb;
  vec3 b0 = texture(uPyr0, vUv).rgb;
  vec3 blur = mix(b1, b2, 0.55);

  // edge aware: lindungi rambut, alis, garis mata, pakaian
  vec2 tx = uSrcTexel * 1.6;
  float gx = luma(texture(uSrc, vUv + vec2(tx.x, 0.0)).rgb) - luma(texture(uSrc, vUv - vec2(tx.x, 0.0)).rgb);
  float gy = luma(texture(uSrc, vUv + vec2(0.0, tx.y)).rgb) - luma(texture(uSrc, vUv - vec2(0.0, tx.y)).rgb);
  float edge = clamp(length(vec2(gx, gy)) * 14.0, 0.0, 1.0);

  float amt = uSmooth * skinLike;
  float keepEdge = mix(0.92, 0.35, uSmoothMode);
  amt *= (1.0 - edge * keepEdge);
  amt = clamp(amt, 0.0, 1.0);

  vec3 smoothed = mix(c, blur, amt);

  // retensi detail: kembalikan high-freq halus (pori) di luar area smoothing penuh
  vec3 detail = c - b0;
  float dl = luma(detail);
  float micro = smoothstep(0.004, 0.05, abs(dl));
  smoothed += detail * (uTexture * (1.0 - amt * 0.85)) * mix(1.0, 0.35, uSmoothMode);
  smoothed += detail * micro * 0.10 * uTexture * skinLike;

  // blemish / noda gelap: piksel jauh lebih gelap dari lokal → tarik ke blur
  float dark = clamp(luma(blur) - luma(c), 0.0, 1.0);
  float spot = smoothstep(0.045, 0.20, dark) * skinLike;
  smoothed = mix(smoothed, blur * 0.995 + vec3(0.002), clamp(spot * uBlemish * 1.6, 0.0, 1.0));

  // ratakan warna: chroma low-freq (redness) → blur
  vec3 base = smoothed;
  float y = luma601(smoothed);
  vec2 ch = toChroma(smoothed);
  vec2 chB = toChroma(blur);
  float even = clamp(uToneEven * skinLike, 0.0, 1.0);
  ch = mix(ch, chB, even * 0.85);
  smoothed = fromChroma(y, ch);
  // kembalikan sedikit luma asli supaya tidak "flat"
  smoothed = mix(smoothed, base, 0.06);

  // cerahkan kulit: curve luma di kulit saja (bukan gain global)
  float wh = clamp(uWhiten * skinLike, 0.0, 1.0);
  float yl = luma(smoothed);
  float yc = yl + wh * (0.30 * yl * (1.0 - yl) + 0.10 * (1.0 - yl));
  smoothed *= clamp(yc / max(yl, 1e-4), 0.8, 1.6);

  // matte / de-shine: rolloff highlight di kulit
  float spec = smoothstep(0.68, 0.99, luma(smoothed)) * clamp(uMatte * skinLike, 0.0, 1.0);
  smoothed -= spec * 0.20;

  // milk glow: angkat low-freq terang secara lembut
  float glowAmt = uGlow * skinLike;
  if (glowAmt > 0.0) {
    float lift = smoothstep(0.42, 0.95, luma(b2)) * glowAmt;
    smoothed += vec3(1.0, 0.97, 0.95) * lift * 0.16 * uGlowAmt;
  }

  outColor = vec4(clamp(smoothed, 0.0, 1.0), 1.0);
}`;

/* ─────────────────────────────────────────────────── features (mata/gigi) */
export const FS_FEATURES = /* glsl */ `
${MASK_SLOTS}
uniform sampler2D uSrc;
uniform sampler2D uPyr1;
uniform vec2 uSrcTexel;
uniform vec2 uEyeC[2];
uniform vec2 uEyeR[2];
uniform float uEyeWhite, uEyeBright, uUndereye, uIrisPop, uCatch, uTeeth, uTeethPolish, uTime;
in vec2 vUv;
out vec4 outColor;

void main() {
  vec3 c = texture(uSrc, vUv).rgb;
  vec4 a = mA(vUv);
  vec4 b = mB(vUv);
  vec2 tx = uSrcTexel;

  /* ── MATA ─────────────────────────────────────── */
  float eye = clamp(a.g, 0.0, 1.0);
  if (eye > 0.003) {
    float y = luma(c);
    float chroma = sat(c);
    // sclera: terang + minim saturasi
    float sclera = smoothstep(0.30, 0.60, y) * (1.0 - smoothstep(0.34, 0.80, chroma)) * eye;
    vec3 white = vec3(0.97, 0.975, 1.0);
    c = mix(c, white, clamp(sclera * uEyeWhite * 0.85, 0.0, 1.0));
    // iris: saturasi + mikrokontras
    float iris = (1.0 - smoothstep(0.16, 0.44, y)) * eye;
    float lum = luma(c);
    c = mix(c, mix(vec3(lum), c, 1.0 + 0.55 * uIrisPop), iris * uIrisPop);
    // unsharp mata (menajamkan tepian iris & bulu mata)
    vec3 nb = (texture(uSrc, vUv + vec2(tx.x, 0.0)).rgb + texture(uSrc, vUv - vec2(tx.x, 0.0)).rgb
             + texture(uSrc, vUv + vec2(0.0, tx.y)).rgb + texture(uSrc, vUv - vec2(0.0, tx.y)).rgb) * 0.25;
    c += (c - nb) * eye * (0.55 * uEyeBright);
    c += vec3(0.06, 0.05, 0.03) * eye * uEyeBright * smoothstep(0.0, 0.35, 0.35 - lum);
    // catchlight: titik refleksi kecil ala ring light
    for (int i = 0; i < 2; i++) {
      vec2 ec = uEyeC[i];
      vec2 er = uEyeR[i];
      vec2 p = (vUv - ec) / max(er, vec2(1e-4));
      float d = length(p - vec2(-0.18, -0.34));
      float cl = exp(-d * d * 6.0) * (1.0 - smoothstep(0.0, 0.35, lum));
      c += vec3(1.0, 0.98, 0.94) * cl * uCatch * 0.55;
    }
  }

  /* ── MATA PANDA / EYE BAG ─────────────────────── */
  float und = clamp(b.r, 0.0, 1.0);
  if (und > 0.003) {
    vec3 blur = texture(uPyr1, vUv).rgb;
    float dDark = clamp(luma(blur) - luma(c), 0.0, 1.0);
    float k = clamp(uUndereye, 0.0, 1.0) * und;
    // cerahkan + kurangi bayangan + samarkan garis
    c = mix(c, max(blur, c * 1.02), clamp(k * (0.45 + dDark * 2.2), 0.0, 1.0));
    float cs = sat(c), cb2 = sat(blur);
    c = mix(c, vec3(luma(c)) * 0.94 + c * 0.06 + blur * 0.10, clamp(k * 0.45 * (cs - cb2 + 0.4), 0.0, 1.0));
    c = c + vec3(0.012, 0.008, 0.004) * k;
  }

  /* ── GIGI ─────────────────────────────────────── */
  float teeth = clamp(a.a, 0.0, 1.0);
  if (teeth > 0.003) {
    vec3 blur = texture(uPyr1, vUv).rgb;
    float y = luma(c);
    float gate = smoothstep(0.16, 0.52, y) * teeth; // lewati celah gelap antar gigi
    float chroma = sat(c);
    // buang kuning: geser ke arah putih netral, lalu naikkan luma
    vec3 neutral = mix(c, vec3(y), clamp(chroma * 2.2, 0.0, 1.0) * 0.8);
    float up = 0.22 + 0.16 * smoothstep(0.4, 0.75, y);
    vec3 w = clamp(neutral + up, 0.0, 1.0) * vec3(1.0, 0.995, 0.985);
    c = mix(c, w, clamp(gate * uTeeth, 0.0, 1.0));
    // polish: ratakan warna antar gigi
    c = mix(c, mix(c, blur, 0.75), clamp(gate * uTeethPolish * 0.6, 0.0, 1.0));
  }

  outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}`;

/* ───────────────────────────────────────────────────────── makeup pass */
export const FS_MAKEUP = /* glsl */ `
${MASK_SLOTS}
uniform sampler2D uSrc;
uniform sampler2D uPyr0;
uniform vec2 uSrcTexel;
uniform float uLashes, uLiner, uBrowFill, uShadow, uBlush, uLipStrength, uLipGloss, uHighlight, uContour, uFreckle, uGlass, uOn;
uniform vec3 uLinerCol, uBrowCol, uShadowCol, uBlushCol, uLipCol, uHiCol, uContourCol;
in vec2 vUv;
out vec4 outColor;

vec3 paint(vec3 base, vec3 col, float amt) {
  // multiply lembut lalu tarik ke warna pigmen → make up terasa "menyatu" dgn tekstur
  float a = clamp(amt, 0.0, 1.0);
  vec3 m = base * (col * 2.0);
  m = mix(base, m, a * 0.7);
  m = mix(m, col, a * 0.45);
  return clamp(m, 0.0, 1.0);
}

void main() {
  vec3 c = texture(uSrc, vUv).rgb;
  vec3 soft = texture(uPyr0, vUv).rgb;
  if (uOn < 0.5) { outColor = vec4(c, 1.0); return; }

  /* eyeliner + bulu mata */
  float lash = clamp(mB(vUv).b, 0.0, 1.0);
  if (lash > 0.002) {
    float k = clamp(max(uLashes, uLiner), 0.0, 1.0);
    c = mix(c, uLinerCol, clamp(lash * k * 1.15, 0.0, 1.0));
  }

  /* alis */
  float brow = clamp(mB(vUv).g, 0.0, 1.0);
  if (brow > 0.002 && uBrowFill > 0.0) {
    vec3 t = paint(c, uBrowCol, brow * uBrowFill * 0.7);
    c = mix(c, t, clamp(brow * uBrowFill, 0.0, 1.0));
  }

  /* eyeshadow di kelopak */
  float lid = clamp(mC(vUv).g, 0.0, 1.0);
  if (lid > 0.002 && uShadow > 0.0) c = paint(c, uShadowCol, lid * uShadow * 0.8);

  /* blush */
  float blush = clamp(mC(vUv).r, 0.0, 1.0);
  if (blush > 0.002 && uBlush > 0.0) {
    float y = luma(c);
    vec3 t = paint(c, uBlushCol, blush * uBlush * 0.75);
    t += vec3(0.03, 0.02, 0.02) * blush * smoothstep(0.45, 0.9, y); // sedikit glow di pipi
    c = t;
  }

  /* bibir */
  float lip = clamp(mA(vUv).b, 0.0, 1.0);
  if (lip > 0.002 && uLipStrength > 0.0) {
    float y = luma(c);
    float inner = 1.0 - smoothstep(0.0, 0.5, abs(y - 0.42));
    vec3 t = mix(c, uLipCol, clamp(lip * uLipStrength * 0.9, 0.0, 1.0));
    t *= mix(1.0, 0.86, lip * uLipStrength * 0.5); // pigmen pekat
    c = t;
    // gloss: specular based on lokal contrast + vertikal gradient di bibir bawah
    float glossMask = lip * (0.35 + 0.65 * smoothstep(0.0, 0.2, vUv.y));
    float spec = smoothstep(0.55, 0.98, luma(soft)) * 0.5 + smoothstep(0.62, 1.0, y) * 0.8;
    c += vec3(1.0, 0.97, 0.96) * clamp(spec * glossMask * uLipGloss * (0.4 + inner), 0.0, 1.0) * 0.5;
    // garis bibir sedikit dipoles
    c -= vec3(0.02) * lip * uLipStrength * smoothstep(0.55, 1.0, lip);
  }

  /* highlighter */
  float hi = clamp(mC(vUv).b, 0.0, 1.0);
  if (hi > 0.002 && uHighlight > 0.0) {
    float y = luma(c);
    float sh = smoothstep(0.30, 0.85, y);
    c = mix(c, c + uHiCol * 0.24, hi * uHighlight * (0.35 + 0.65 * sh));
    c = mix(c, soft, hi * uHighlight * 0.18);
  }

  /* contour */
  float co = clamp(mC(vUv).a, 0.0, 1.0);
  if (co > 0.002 && uContour > 0.0) {
    c = mix(c, c * uContourCol * 1.9, co * uContour * 0.55);
  }

  /* freckle */
  if (uFreckle > 0.001) {
    float skin = skinMask(vUv);
    vec2 g = vUv / 0.006;
    float cell = hash12(floor(g));
    vec2 f = fract(g) - 0.5;
    vec2 jit = vec2(hash12(floor(g) + 11.0), hash12(floor(g) + 37.0)) - 0.5;
    float d = length(f - jit * 0.7);
    float dotm = (1.0 - smoothstep(0.10, 0.30, d)) * step(0.90, cell);
    float zone = smoothstep(0.55, 0.0, abs(vUv.y - 0.5)) * max(clamp(mC(vUv).r, 0.0, 1.0), clamp(mB(vUv).a, 0.0, 1.0) * 0.8);
    c = mix(c, c * vec3(0.72, 0.58, 0.46), clamp(dotm * skin * zone * uFreckle, 0.0, 1.0));
  }

  /* glass skin veil */
  if (uGlass > 0.001) {
    float skin = skinMask(vUv);
    float y = luma(c);
    float sheen = smoothstep(0.52, 0.95, y);
    c += vec3(0.05, 0.045, 0.045) * skin * uGlass * (0.35 + 0.65 * sheen);
    c = mix(c, mix(c, soft, 0.30), skin * uGlass * 0.5);
  }

  outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}`;

/* ────────────────────────────────────────────────────────── bloom pass */
export const FS_BRIGHT = /* glsl */ `
uniform sampler2D uTex;
uniform float uThresh, uSoft;
in vec2 vUv;
out vec4 outColor;
void main() {
  vec3 c = texture(uTex, vUv).rgb;
  float y = luma(c);
  float k = smoothstep(uThresh, uThresh + max(uSoft, 0.01), y);
  outColor = vec4(c * k, 1.0);
}`;

/* ──────────────────────────────────────────────────────── grade / look */
export const FS_GRADE = /* glsl */ `
${MASK_SLOTS}
uniform sampler2D uSrc;
uniform sampler2D uPyr1;
uniform sampler2D uBloom;
uniform mediump sampler3D uLut;
uniform float uLutSize;
uniform vec2 uRes;
uniform float uExposure, uContrast, uSat, uTemp, uTint, uHi, uSh, uFade, uVig, uGrain, uBloomAmt, uHalation, uSoft, uSharp, uClarity, uRing, uTime, uLutMix, uHasLut, uAutoExp;
in vec2 vUv;
out vec4 outColor;

vec3 gradeTemp(vec3 c, float t) {
  return clamp(c * vec3(1.0 + 0.16 * t, 1.0 + 0.02 * t, 1.0 - 0.16 * t), 0.0, 4.0);
}

void main() {
  vec3 c = texture(uSrc, vUv).rgb;
  vec3 base = c;
  float skin = skinMask(vUv);

  /* ring light: fill radial lembut + sedikit lift bayangan di wajah */
  if (uRing > 0.001) {
    vec2 p = (vUv - 0.5) * vec2(uRes.x / uRes.y, 1.0);
    float r = length(p);
    float fill = (1.0 - smoothstep(0.15, 0.95, r)) * uRing;
    float y = luma(c);
    float lift = fill * (0.10 + 0.22 * (1.0 - y));
    c += vec3(1.0, 0.98, 0.95) * lift * mix(0.55, 1.0, skin);
    // halo cincin tipis khas ring light
    c += vec3(0.9, 0.95, 1.0) * smoothstep(0.02, 0.0, abs(r - 0.62)) * uRing * 0.03;
  }

  /* soft focus / diffusion: campur blur, tapi tahan highlight biar "creamy" */
  if (uSoft > 0.001) {
    vec3 b = texture(uPyr1, vUv).rgb;
    float y = luma(c);
    float k = uSoft * (0.45 + 0.55 * smoothstep(0.55, 0.95, y));
    c = mix(c, mix(b, c, 0.25), clamp(k * 0.55, 0.0, 0.85));
  }

  /* clarity / sharpen pakai luma pyramid */
  vec3 pyr = texture(uPyr1, vUv).rgb;
  float yb = luma(base);
  float y0 = luma(pyr);
  float detail = yb - y0;
  c += vec3(detail) * (uClarity * 0.9) * (1.0 - smoothstep(0.03, 0.30, abs(detail)) * 0.45);
  if (uSharp > 0.001) {
    vec2 tx = 1.0 / uRes;
    vec3 nb = (texture(uSrc, vUv + vec2(tx.x, 0.0)).rgb + texture(uSrc, vUv - vec2(tx.x, 0.0)).rgb
             + texture(uSrc, vUv + vec2(0.0, tx.y)).rgb + texture(uSrc, vUv - vec2(0.0, tx.y)).rgb) * 0.25;
    c += (c - nb) * uSharp * 0.85;
  }

  /* bloom + halation */
  if (uBloomAmt > 0.001 || uHalation > 0.001) {
    vec3 bl = texture(uBloom, vUv).rgb;
    c += bl * uBloomAmt * 0.85;
    float hy = luma(bl);
    vec3 halo = bl * vec3(1.25, 0.72, 0.45) * smoothstep(0.15, 0.7, hy);
    c += halo * uHalation * 0.9;
  }

  /* tonemap sederhana supaya highlight tidak clip setelah semua additif */
  c = c / (1.0 + 0.10 * max(0.0, luma(c) - 0.85));

  /* exposure / temp / tint / sat / contrast / hi-lo */
  c *= pow(2.0, uExposure * 0.9 + uAutoExp);
  c = gradeTemp(c, uTemp);
  c *= vec3(1.0 - 0.06 * uTint, 1.0 + 0.10 * uTint, 1.0 - 0.04 * abs(uTint) + 0.06 * uTint);
  float y = luma(c);
  c = mix(vec3(y), c, clamp(1.0 + uSat, 0.0, 2.0));
  c = (c - 0.5) * (1.0 + uContrast * 0.55) + 0.5;
  float yy = luma(c);
  vec3 tone = mix(c, c * vec3(1.06, 1.03, 1.0), uHi * smoothstep(0.55, 1.0, yy));
  tone = mix(tone, tone * vec3(0.94, 0.96, 1.02), -uSh * (1.0 - smoothstep(0.0, 0.55, yy)));
  c = tone;
  c += uSh * 0.06 * (1.0 - smoothstep(0.0, 0.6, yy));

  /* LUT eksternal */
  if (uHasLut > 0.5 && uLutMix > 0.001) {
    vec3 q = clamp(c, 0.0, 1.0) * (uLutSize - 1.0) / uLutSize + 0.5 / uLutSize;
    vec3 lut = texture(uLut, q).rgb;
    c = mix(c, lut, uLutMix);
  }

  /* fade (lifted blacks) + vignette */
  c = c * (1.0 - uFade * 0.22) + uFade * 0.055;
  vec2 p = (vUv - 0.5) * vec2(uRes.x / uRes.y, 1.0);
  float r = length(p);
  c *= 1.0 - uVig * smoothstep(0.42, 1.15, r) * 0.85;
  c = clamp(c, 0.0, 1.0);

  /* grain halus (anisotropik, sedikit berwarna) */
  if (uGrain > 0.001) {
    float g1 = hash12(vUv * uRes + vec2(uTime * 37.0, uTime * 11.0));
    float g2 = hash12(vUv * uRes * 0.5 + vec2(uTime * 13.0, uTime * 71.0));
    float g = (g1 * 0.7 + g2 * 0.3) - 0.5;
    float amt = uGrain * (0.35 + 0.65 * (1.0 - smoothstep(0.0, 0.85, luma(c))));
    c += vec3(1.0, 0.98, 0.95) * g * amt * 0.16;
  }

  outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}`;

/* ─────────────────────────────────────────────────── present / composite */
export const FS_PRESENT = /* glsl */ `
uniform sampler2D uSrc;
uniform sampler2D uOverlay;
uniform sampler2D uRaw;
uniform vec2 uRes;
uniform float uSplit, uFlash;
in vec2 vUv;
out vec4 outColor;
void main() {
  vec2 uv = vUv;
  vec3 c = texture(uSrc, uv).rgb;
  // compare: area kiri = frame asli (tanpa beautify & tanpa warp)
  if (uSplit > 0.0) {
    float k = step(uv.x, uSplit);
    c = mix(c, texture(uRaw, vUv).rgb, k);
    if (abs(uv.x - uSplit) < 0.0016) c = mix(c, vec3(1.0), 0.7);
  }
  vec4 o = texture(uOverlay, vUv);
  c = mix(c, o.rgb, o.a);
  c = mix(c, vec3(1.0), uFlash);
  outColor = vec4(c, 1.0);
}`;

export const FS_IDENTITY = /* glsl */ `
uniform sampler2D uTex;
in vec2 vUv;
out vec4 outColor;
void main() { outColor = vec4(texture(uTex, vUv).rgb, 1.0); }`;
