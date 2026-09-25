/**
 * Semua parameter real-time HaoCam + preset.
 *
 * Nilai 0..1 kecuali dinyatakan lain, supaya slider di UI selalu konsisten.
 * Struktur ini yang dikirim tiap frame ke GPU (flat, tanpa alokasi baru) —
 * persis pola "parameter block" di engine beauty camera native.
 */

export type Aspect = '9:16' | '4:5' | '1:1' | '16:9' | 'full';
export type Quality = 'power' | 'balanced' | 'ultra';

export interface Vec3 {
  r: number;
  g: number;
  b: number;
}

export interface FxParams {
  /* ── Skin ─────────────────────────────────────────────────── */
  smooth: number; // kekuatan smoothing (0..1)
  smoothRadius: number; // 1..4 → skala pyramid blur (detail retention)
  smoothMode: number; // 0 = surface (edge aware), 1 = airbrush (lembut penuh)
  texture: number; // 0..1 pori/detail yang dipertahankan
  whitening: number; // 0..1 cerahkan kulit (curve luma, bukan gain global)
  toneEven: number; // 0..1 ratakan warna (redness/chroma low-freq)
  blemish: number; // 0..1 hapus noda jerawat/gelanggang gelap
  matte: number; // 0..1 kurangi kilap (highlight rolloff di kulit)
  glow: number; // 0..1 skin glow / "milk" diffusion

  /* ── Reshape (mesh warp) ──────────────────────────────────── */
  jawSlim: number; // -0.5..1
  cheek: number; // 0..1 tulang pipi
  chin: number; // -0.5..0.5 panjang/pendek dagu
  forehead: number; // -0.5..0.5 tinggi jidat
  eyeEnlarge: number; // 0..1
  eyeSpacing: number; // -0.5..0.5
  noseSlim: number; // 0..1
  noseShort: number; // -0.5..0.5
  lipPlump: number; // 0..1
  smile: number; // 0..1 auto angkat sudut mulut (dipicu ekspresi)
  browLift: number; // 0..1
  earFade: number; // 0..1 "face taper" ke arah telinga

  /* ── Mata & gigi ──────────────────────────────────────────── */
  eyeWhite: number; // 0..1 putihkan sclera
  eyeBright: number; // 0..1 cerahkan mata + hilangkan mata panda
  undereye: number; // 0..1 dark circle / eye bag
  irisPop: number; // 0..1 saturasi + sharp iris
  lashes: number; // 0..1 bulu mata (dibangun dari spline kelopak)
  liner: number; // 0..1 eyeliner
  catchlight: number; // 0..1 titik cahaya di iris
  teeth: number; // 0..1 whitening gigi
  teethPolish: number; // 0..1 ratakan warna gigi

  /* ── Makeup ───────────────────────────────────────────────── */
  makeupOn: number; // 0/1
  blush: number; // 0..1
  blushColor: Vec3;
  lipColor: Vec3;
  lipStrength: number; // 0..1
  lipGloss: number; // 0..1
  shadowColor: Vec3;
  shadow: number; // 0..1 eyeshadow
  browColor: Vec3;
  browFill: number; // 0..1
  highlight: number; // 0..1 highlighter tulang pipi/jembatan hidung
  contour: number; // 0..1 shading sisi wajah
  freckle: number; // 0..1
  tint: number; // 0..1 "glass skin" tint (luminous veil)

  /* ── Light & grade ────────────────────────────────────────── */
  ringLight: number; // 0..1 ring light (fill + catchlight)
  softFocus: number; // 0..1 diffusion (bloom di kulit)
  bloom: number; // 0..1 glow highlight
  halation: number; // 0..1 red/orange spill di area terang (film)
  exposure: number; // -1..1
  contrast: number; // -1..1
  saturation: number; // -1..1
  temp: number; // -1..1 (hangat/dingin)
  tint2: number; // -1..1 (magenta/green)
  highlights: number; // -1..1
  shadows: number; // -1..1
  fade: number; // 0..1 lifted blacks
  grain: number; // 0..1 film grain
  vignette: number; // 0..1
  sharpness: number; // 0..1 unsharp mask global
  clarity: number; // -1..1 local contrast
  lutStrength: number; // 0..1 LUT eksternal (.cube)
  lutMode: number; // 0/1 tersedia

  /* ── Transform & AR ───────────────────────────────────────── */
  zoom: number; // 1..2.5
  rotate: number; // 0/90/180/270
  aspect: Aspect;
  aspectRatio: number; // dihitung dari aspect (w/h)
  mirror: number; // 0/1
  frame: number; // index daftar frame
  frameAlpha: number; // 0..1
  lens: number; // index daftar lens AR (-1 = off)
  lensIntensity: number; // 0..1
  autoTrigger: number; // 0..1 reaksi lens ke ekspresi (senyum/buka mulut)
}

export const DEFAULT_PARAMS: FxParams = {
  smooth: 0.55,
  smoothRadius: 1.9,
  smoothMode: 0,
  texture: 0.55,
  whitening: 0.18,
  toneEven: 0.45,
  blemish: 0.35,
  matte: 0.2,
  glow: 0.25,

  jawSlim: 0.35,
  cheek: 0.18,
  chin: 0,
  forehead: 0,
  eyeEnlarge: 0.28,
  eyeSpacing: 0,
  noseSlim: 0.2,
  noseShort: 0,
  lipPlump: 0.15,
  smile: 0.2,
  browLift: 0.1,
  earFade: 0.12,

  eyeWhite: 0.35,
  eyeBright: 0.4,
  undereye: 0.45,
  irisPop: 0.3,
  lashes: 0.4,
  liner: 0.2,
  catchlight: 0.3,
  teeth: 0.4,
  teethPolish: 0.3,

  makeupOn: 0,
  blush: 0,
  blushColor: { r: 0.94, g: 0.45, b: 0.5 },
  lipColor: { r: 0.82, g: 0.18, b: 0.24 },
  lipStrength: 0,
  lipGloss: 0.3,
  shadowColor: { r: 0.66, g: 0.42, b: 0.44 },
  shadow: 0,
  browColor: { r: 0.28, g: 0.19, b: 0.16 },
  browFill: 0,
  highlight: 0.12,
  contour: 0.15,
  freckle: 0,
  tint: 0.1,

  ringLight: 0.22,
  softFocus: 0.18,
  bloom: 0.22,
  halation: 0.12,
  exposure: 0.06,
  contrast: 0.08,
  saturation: 0.05,
  temp: 0.05,
  tint2: 0,
  highlights: -0.1,
  shadows: 0.12,
  fade: 0.05,
  grain: 0.05,
  vignette: 0.1,
  sharpness: 0.35,
  clarity: 0.12,
  lutStrength: 0.85,
  lutMode: 0,

  zoom: 1,
  rotate: 0,
  aspect: '9:16',
  aspectRatio: 9 / 16,
  mirror: 1,
  frame: 0,
  frameAlpha: 1,
  lens: -1,
  lensIntensity: 1,
  autoTrigger: 0.5,
};

/** baseline grading netral — dipakai saat mengganti filter supaya tidak menumpuk */
export const NEUTRAL_GRADE: Partial<FxParams> = {
  exposure: 0,
  contrast: 0,
  saturation: 0,
  temp: 0,
  tint2: 0,
  highlights: 0,
  shadows: 0,
  fade: 0,
  grain: 0,
  vignette: 0,
  bloom: 0,
  halation: 0,
  softFocus: 0,
  clarity: 0,
  lutStrength: 0.85,
};

/** preset beauty instan ala "Auto" di app beauty */
export interface BeautyPreset {
  id: string;
  name: string;
  hint: string;
  patch: Partial<FxParams>;
}

export const BEAUTY_PRESETS: BeautyPreset[] = [
  {
    id: 'none',
    name: 'Raw',
    hint: 'Tanpa beautify — benar-benar mentah',
    patch: {
      smooth: 0,
      texture: 1,
      whitening: 0,
      toneEven: 0,
      blemish: 0,
      matte: 0,
      glow: 0,
      jawSlim: 0,
      cheek: 0,
      eyeEnlarge: 0,
      noseSlim: 0,
      lipPlump: 0,
      smile: 0,
      browLift: 0,
      earFade: 0,
      eyeWhite: 0,
      eyeBright: 0,
      undereye: 0,
      irisPop: 0,
      lashes: 0,
      liner: 0,
      teeth: 0,
      sharpness: 0,
      softFocus: 0,
      ringLight: 0,
      contrast: 0,
      saturation: 0,
      exposure: 0,
      grain: 0,
      vignette: 0,
      fade: 0,
      bloom: 0,
      halation: 0,
      clarity: 0,
      tint: 0,
      highlight: 0,
      contour: 0,
      makeupOn: 0,
      blush: 0,
      lipStrength: 0,
      shadow: 0,
      browFill: 0,
    },
  },
  {
    id: 'auto',
    name: 'Auto',
    hint: 'Kulit bersih, detail mata & gigi tetap tajam',
    patch: { smooth: 0.55, texture: 0.6, toneEven: 0.45, whitening: 0.15, blemish: 0.35, eyeBright: 0.4, teeth: 0.35, sharpness: 0.35, softFocus: 0.15, jawSlim: 0.3, eyeEnlarge: 0.22 },
  },
  {
    id: 'korean',
    name: 'Korean Milk',
    hint: 'Kulit susu, glow lembut, low contrast',
    patch: {
      smooth: 0.82,
      smoothMode: 1,
      texture: 0.42,
      whitening: 0.4,
      toneEven: 0.7,
      glow: 0.5,
      matte: 0.35,
      contrast: -0.05,
      saturation: -0.06,
      temp: -0.05,
      exposure: 0.16,
      fade: 0.12,
      softFocus: 0.4,
      ringLight: 0.45,
      undereye: 0.6,
      teeth: 0.45,
      jawSlim: 0.5,
      cheek: 0.2,
      lipStrength: 0.35,
      makeupOn: 1,
      blush: 0.25,
      grain: 0.02,
      vignette: 0.04,
    },
  },
  {
    id: 'prism',
    name: 'Prism Soft',
    hint: 'Signature look: skin diffusion + halo',
    patch: {
      smooth: 0.7,
      texture: 0.5,
      glow: 0.55,
      softFocus: 0.5,
      bloom: 0.5,
      halation: 0.25,
      contrast: 0.02,
      highlights: -0.2,
      exposure: 0.08,
      sharpness: 0.45,
      clarity: 0.18,
      eyeEnlarge: 0.3,
      lashes: 0.55,
      catchlight: 0.5,
      teeth: 0.5,
      highlight: 0.3,
      grain: 0.04,
      vignette: 0.12,
    },
  },
  {
    id: 'procelain',
    name: 'Porcelain',
    hint: 'Matte mulus tanpa kilap',
    patch: { smooth: 0.9, smoothMode: 1, texture: 0.3, matte: 0.75, whitening: 0.35, toneEven: 0.8, blemish: 0.7, glow: 0.2, softFocus: 0.3, contrast: 0.12, saturation: -0.1 },
  },
  {
    id: 'film',
    name: 'Cinema Skin',
    hint: 'Film grain + halation, skin lembut natural',
    patch: {
      smooth: 0.42,
      texture: 0.75,
      toneEven: 0.3,
      grain: 0.42,
      halation: 0.5,
      bloom: 0.3,
      fade: 0.22,
      contrast: 0.2,
      saturation: -0.12,
      temp: 0.14,
      highlights: -0.3,
      shadows: 0.2,
      vignette: 0.3,
      sharpness: 0.25,
      softFocus: 0.2,
    },
  },
  {
    id: 'boy',
    name: 'Groomed (mask)',
    hint: 'Detail dipertahankan, hanya kulit & mata dirapikan',
    patch: { smooth: 0.34, texture: 0.85, toneEven: 0.32, blemish: 0.5, matte: 0.45, whitening: 0.04, eyeBright: 0.35, undereye: 0.5, browFill: 0.25, makeupOn: 1, blush: 0, lipStrength: 0.08, jawSlim: 0.28, cheek: 0.25, eyeEnlarge: 0.12, lashes: 0.15, contour: 0.25, softFocus: 0.08, grain: 0.08, saturation: -0.02 },
  },
];

/** Filter/grading preset: semua nilai grade di-override, beauty tetap. */
export interface FilterPreset {
  id: string;
  name: string;
  emoji: string;
  patch: Partial<FxParams>;
}

export const FILTERS: FilterPreset[] = [
  { id: 'orig', name: 'Original', emoji: '⚪', patch: {} },
  {
    id: 'golden',
    name: 'Golden Hour',
    emoji: '🌇',
    patch: { temp: 0.42, highlights: -0.18, shadows: 0.22, saturation: 0.14, contrast: 0.14, halation: 0.4, bloom: 0.32, fade: 0.08, vignette: 0.18, exposure: 0.04 },
  },
  {
    id: 'peach',
    name: 'Peach Soda',
    emoji: '🍑',
    patch: { temp: 0.2, tint2: 0.12, saturation: 0.2, contrast: -0.02, fade: 0.16, exposure: 0.1, bloom: 0.28, softFocus: 0.28, vignette: 0.06 },
  },
  {
    id: 'cold',
    name: 'Cold Brew',
    emoji: '🧊',
    patch: { temp: -0.36, saturation: -0.08, contrast: 0.24, shadows: -0.1, highlights: -0.25, clarity: 0.3, vignette: 0.22, fade: 0.04 },
  },
  {
    id: 'matcha',
    name: 'Matcha',
    emoji: '🍵',
    patch: { temp: -0.06, tint2: -0.3, saturation: -0.02, contrast: 0.1, fade: 0.2, highlights: -0.1, grain: 0.12 },
  },
  {
    id: 'sakura',
    name: 'Sakura',
    emoji: '🌸',
    patch: { tint2: 0.3, temp: 0.12, saturation: 0.12, contrast: -0.06, fade: 0.24, bloom: 0.4, softFocus: 0.34, exposure: 0.12 },
  },
  {
    id: 'noir',
    name: 'Noir',
    emoji: '⚫',
    patch: { saturation: -1, contrast: 0.42, clarity: 0.35, grain: 0.3, vignette: 0.4, fade: 0.06, highlights: -0.2, shadows: -0.12 },
  },
  {
    id: 'retro',
    name: 'Retro DV',
    emoji: '📼',
    patch: { contrast: 0.22, saturation: 0.22, temp: 0.18, fade: 0.3, grain: 0.28, halation: 0.45, vignette: 0.34 },
  },
  {
    id: 'chrome',
    name: 'Y2K Chrome',
    emoji: '🫧',
    patch: { contrast: 0.3, saturation: -0.18, temp: -0.12, tint2: 0.16, highlights: 0.25, bloom: 0.5, clarity: 0.4, grain: 0.16 },
  },
  {
    id: 'milktea',
    name: 'Milk Tea',
    emoji: '🧋',
    patch: { temp: 0.24, saturation: -0.12, contrast: 0.05, fade: 0.26, exposure: 0.12, highlights: -0.12, vignette: 0.1, softFocus: 0.24 },
  },
  {
    id: 'kodak',
    name: 'Kodak 400',
    emoji: '🎞️',
    patch: { saturation: 0.1, contrast: 0.16, temp: 0.1, fade: 0.18, grain: 0.24, halation: 0.28, shadows: 0.26, highlights: -0.2 },
  },
  {
    id: 'bleach',
    name: 'Bleach',
    emoji: '🤍',
    patch: { exposure: 0.32, saturation: -0.45, contrast: 0.35, highlights: 0.3 },
  },
];

/** Lens AR (dibuat prosedural, jadi tidak butuh aset eksternal). */
export interface LensDef {
  id: string;
  name: string;
  emoji: string;
}
export const LENSES: LensDef[] = [
  { id: 'none', name: 'Off', emoji: '🚫' },
  { id: 'cat', name: 'Cat', emoji: '🐱' },
  { id: 'bunny', name: 'Bunny', emoji: '🐰' },
  { id: 'halo', name: 'Halo', emoji: '😇' },
  { id: 'crown', name: 'Crown', emoji: '👑' },
  { id: 'hearts', name: 'Heart Eyes', emoji: '😍' },
  { id: 'shades', name: 'Shades', emoji: '🕶️' },
  { id: 'glitter', name: 'Glitter', emoji: '✨' },
  { id: 'tears', name: 'Star Tears', emoji: '🥹' },
  { id: 'whiskers', name: 'Whiskers', emoji: '🐭' },
  { id: 'mecha', name: 'Mecha Scan', emoji: '🤖' },
  { id: 'bubble', name: 'Bubblegum', emoji: '🫦' },
];

export interface FrameDef {
  id: string;
  name: string;
  emoji: string;
}
export const FRAMES: FrameDef[] = [
  { id: 'none', name: 'Tanpa frame', emoji: '⬜' },
  { id: 'polaroid', name: 'Instant', emoji: '🖼️' },
  { id: 'filmstrip', name: 'Film Strip', emoji: '🎞️' },
  { id: 'haocam', name: 'Shot on HaoCam', emoji: '📸' },
  { id: 'softvign', name: 'Vignette Only', emoji: '🌑' },
  { id: 'ui916', name: 'Story UI', emoji: '📱' },
];

export const ASPECTS: { id: Aspect; ratio: number; label: string }[] = [
  { id: '9:16', ratio: 9 / 16, label: '9:16' },
  { id: '4:5', ratio: 4 / 5, label: '4:5' },
  { id: '1:1', ratio: 1, label: '1:1' },
  { id: '16:9', ratio: 16 / 9, label: '16:9' },
  { id: 'full', ratio: 0, label: 'Full' },
];

export interface SliderDef {
  key: keyof FxParams;
  label: string;
  min: number;
  max: number;
  step?: number;
}

export const TAB_SLIDERS: Record<string, SliderDef[]> = {
  skin: [
    { key: 'smooth', label: 'Smoothing', min: 0, max: 1 },
    { key: 'texture', label: 'Retain Detail', min: 0, max: 1 },
    { key: 'toneEven', label: 'Even Tone', min: 0, max: 1 },
    { key: 'blemish', label: 'Blemish Fix', min: 0, max: 1 },
    { key: 'whitening', label: 'Brighten', min: 0, max: 1 },
    { key: 'matte', label: 'Matte / De-shine', min: 0, max: 1 },
    { key: 'glow', label: 'Glow', min: 0, max: 1 },
    { key: 'smoothRadius', label: 'Blur Scale', min: 1, max: 4 },
  ],
  reshape: [
    { key: 'jawSlim', label: 'V-Line / Rahang', min: -0.5, max: 1 },
    { key: 'cheek', label: 'Cheekbone', min: 0, max: 1 },
    { key: 'chin', label: 'Chin Length', min: -0.5, max: 0.5 },
    { key: 'forehead', label: 'Forehead', min: -0.5, max: 0.5 },
    { key: 'eyeEnlarge', label: 'Eye Size', min: 0, max: 1 },
    { key: 'eyeSpacing', label: 'Eye Spacing', min: -0.5, max: 0.5 },
    { key: 'noseSlim', label: 'Nose Slim', min: 0, max: 1 },
    { key: 'noseShort', label: 'Nose Length', min: -0.5, max: 0.5 },
    { key: 'lipPlump', label: 'Lip Plump', min: 0, max: 1 },
    { key: 'smile', label: 'Auto Smile', min: 0, max: 1 },
    { key: 'browLift', label: 'Brow Lift', min: 0, max: 1 },
    { key: 'earFade', label: 'Face Taper', min: 0, max: 1 },
  ],
  eyes: [
    { key: 'eyeBright', label: 'Eye Bright', min: 0, max: 1 },
    { key: 'eyeWhite', label: 'Sclera Whitening', min: 0, max: 1 },
    { key: 'irisPop', label: 'Iris Pop', min: 0, max: 1 },
    { key: 'undereye', label: 'Dark Circle', min: 0, max: 1 },
    { key: 'lashes', label: 'Lashes', min: 0, max: 1 },
    { key: 'liner', label: 'Eyeliner', min: 0, max: 1 },
    { key: 'catchlight', label: 'Catchlight', min: 0, max: 1 },
    { key: 'teeth', label: 'Teeth', min: 0, max: 1 },
    { key: 'teethPolish', label: 'Teeth Even', min: 0, max: 1 },
  ],
  makeup: [
    { key: 'blush', label: 'Blush', min: 0, max: 1 },
    { key: 'lipStrength', label: 'Lip Tint', min: 0, max: 1 },
    { key: 'lipGloss', label: 'Lip Gloss', min: 0, max: 1 },
    { key: 'shadow', label: 'Eyeshadow', min: 0, max: 1 },
    { key: 'browFill', label: 'Brow Fill', min: 0, max: 1 },
    { key: 'highlight', label: 'Highlighter', min: 0, max: 1 },
    { key: 'contour', label: 'Contour', min: 0, max: 1 },
    { key: 'freckle', label: 'Freckle', min: 0, max: 1 },
    { key: 'tint', label: 'Glass Skin', min: 0, max: 1 },
  ],
  light: [
    { key: 'ringLight', label: 'Ring Light', min: 0, max: 1 },
    { key: 'softFocus', label: 'Soft Focus', min: 0, max: 1 },
    { key: 'bloom', label: 'Bloom', min: 0, max: 1 },
    { key: 'halation', label: 'Halation', min: 0, max: 1 },
    { key: 'exposure', label: 'Exposure', min: -1, max: 1 },
    { key: 'contrast', label: 'Contrast', min: -1, max: 1 },
    { key: 'saturation', label: 'Saturation', min: -1, max: 1 },
    { key: 'temp', label: 'Temperature', min: -1, max: 1 },
    { key: 'tint2', label: 'Tint', min: -1, max: 1 },
    { key: 'highlights', label: 'Highlights', min: -1, max: 1 },
    { key: 'shadows', label: 'Shadows', min: -1, max: 1 },
    { key: 'fade', label: 'Fade', min: 0, max: 1 },
    { key: 'clarity', label: 'Clarity', min: -1, max: 1 },
    { key: 'sharpness', label: 'Sharpen', min: 0, max: 1 },
    { key: 'grain', label: 'Grain', min: 0, max: 1 },
    { key: 'vignette', label: 'Vignette', min: 0, max: 1 },
    { key: 'lutStrength', label: 'LUT Mix', min: 0, max: 1 },
  ],
};

/** mode render → menentukan resolusi internal & laju tracker */
export const QUALITY: Record<Quality, { scale: number; maskScale: number; trackEvery: number; label: string }> = {
  power: { scale: 0.62, maskScale: 0.35, trackEvery: 3, label: 'Hemat' },
  balanced: { scale: 0.82, maskScale: 0.5, trackEvery: 2, label: 'Seimbang' },
  ultra: { scale: 1, maskScale: 0.7, trackEvery: 1, label: 'Ultra' },
};
