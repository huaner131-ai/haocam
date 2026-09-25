/**
 * iBUG-68 face landmark: indeks, region, dan turunan geometri (roll/yaw/pitch).
 * Ini fondasi semua efek: smoothing, mask region, makeup, dan mesh warp.
 */

export const N_POINTS = 68;

export const IDX = {
  jaw: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
  browR: [17, 18, 19, 20, 21], // alis kiri-di-layar (subjek kanan)
  browL: [22, 23, 24, 25, 26],
  noseBridge: [27, 28, 29, 30],
  noseBase: [31, 32, 33, 34, 35],
  eyeR: [36, 37, 38, 39, 40, 41],
  eyeL: [42, 43, 44, 45, 46, 47],
  lipOuter: [48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59],
  lipInner: [60, 61, 62, 63, 64, 65, 66, 67],
} as const;

export const RIGHT_EYE = {
  outer: 36,
  top: [37, 38],
  inner: 39,
  bottom: [40, 41],
  upper: [36, 37, 38, 39],
  lower: [39, 40, 41, 36],
} as const;
export const LEFT_EYE = {
  inner: 42,
  top: [43, 44],
  outer: 45,
  bottom: [46, 47],
  upper: [42, 43, 44, 45],
  lower: [45, 46, 47, 42],
} as const;

export type Pt = { x: number; y: number };

export function get(pts: Float32Array, i: number): Pt {
  return { x: pts[i * 2], y: pts[i * 2 + 1] };
}
export function mid(a: Pt, b: Pt): Pt {
  return { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 };
}
export function centroid(pts: Float32Array, idx: readonly number[]): Pt {
  let x = 0,
    y = 0;
  for (const i of idx) {
    x += pts[i * 2];
    y += pts[i * 2 + 1];
  }
  return { x: x / idx.length, y: y / idx.length };
}
export function dist(a: Pt, b: Pt) {
  return Math.hypot(a.x - b.x, a.y - b.y);
}

/** semua turunan geometri yang dibutuhkan pipeline */
export interface FaceGeom {
  pts: Float32Array; // 68*2, y-down normalized (0..1)
  vis: number; // 0..1 (fade in/out halus)
  box: [number, number, number, number]; // x,y,w,h normalized
  center: Pt;
  /** lebar wajah normalized (antaran pelipis) — dipakai untuk skala semua efek */
  width: number;
  roll: number; // radian (kemiringan garis mata)
  yaw: number; // -1..1 (profile guard)
  pitch: number; // -1..1
  eyeR: { c: Pt; rx: number; ry: number; angle: number };
  eyeL: { c: Pt; rx: number; ry: number; angle: number };
  mouth: { c: Pt; rx: number; ry: number };
  happy: number;
  surprised: number;
  blink: number;
}

export function eyeInfo(pts: Float32Array, which: 'R' | 'L') {
  const eye = which === 'R' ? IDX.eyeR : IDX.eyeL;
  const c = centroid(pts, eye);
  const outer = which === 'R' ? get(pts, RIGHT_EYE.outer) : get(pts, LEFT_EYE.outer);
  const inner = which === 'R' ? get(pts, RIGHT_EYE.inner) : get(pts, LEFT_EYE.inner);
  const top = centroid(pts, which === 'R' ? RIGHT_EYE.top : LEFT_EYE.top);
  const bot = centroid(pts, which === 'R' ? RIGHT_EYE.bottom : LEFT_EYE.bottom);
  const rx = dist(outer, inner) * 0.5;
  const ry = dist(top, bot) * 0.5;
  const angle = Math.atan2(inner.y - outer.y, inner.x - outer.x);
  return { c, rx: Math.max(rx, 1e-4), ry: Math.max(ry, 1e-4), angle };
}

export function mouthInfo(pts: Float32Array) {
  const c = centroid(pts, IDX.lipOuter);
  const rc = get(pts, 48);
  const lc = get(pts, 54);
  const top = centroid(pts, [51, 52]);
  const bot = centroid(pts, [57, 58]);
  return { c, rx: Math.max(dist(rc, lc) * 0.5, 1e-4), ry: Math.max(dist(top, bot) * 0.5, 1e-4) };
}

/**
 * Estimasi pose kepala murni dari geometri (tanpa model pose).
 * roll  : atan2 garis antar-pusat mata
 * yaw   : offset horizontal ujung hidung terhadap titik tengah garis mata, dinormalisasi
 * pitch : rasio tinggi wajah tengah (mata→dagu vs brow→mata)
 */
export function headPose(pts: Float32Array, width: number) {
  const er = eyeInfo(pts, 'R');
  const el = eyeInfo(pts, 'L');
  const roll = Math.atan2(el.c.y - er.c.y, el.c.x - er.c.x);
  const eyeMid = mid(er.c, el.c);
  const noseTip = get(pts, 33);
  const brow = mid(centroid(pts, IDX.browR), centroid(pts, IDX.browL));
  const chin = get(pts, 8);
  const half = Math.max(width * 0.5, 1e-4);
  const yaw = clamp((noseTip.x - eyeMid.x) / (half * 0.55), -1, 1);
  const up = dist(eyeMid, brow);
  const down = dist(eyeMid, chin);
  const pitch = clamp((down / Math.max(up + down, 1e-4) - 0.56) * 3.2, -1, 1);
  return { roll, yaw, pitch };
}

export function clamp(v: number, a: number, b: number) {
  return v < a ? a : v > b ? b : v;
}
export function lerp(a: number, b: number, t: number) {
  return a + (b - a) * t;
}

/**
 * Polygon wajah untuk mask "kulit": hull 68 titik tidak menutupi jidat,
 * jadi kita bangun ulang kontur + extrapolasi ke garis rambut.
 */
export function faceHull(pts: Float32Array, out: number[]): number {
  const browR = IDX.browR;
  const browL = IDX.browL;
  const b0 = get(pts, browR[0]); // pelipis kanan
  const b6 = get(pts, browL[4]); // pelipis kiri
  const bMid = mid(centroid(pts, browR), centroid(pts, browL));
  const noseTip = get(pts, 33);
  const eyeR = eyeInfo(pts, 'R').c;
  const eyeL = eyeInfo(pts, 'L').c;

  const faceH = Math.abs(get(pts, 8).y - bMid.y);
  const up = faceH * 0.52; // tinggi jidat yang diasumsikan
  const spread = Math.abs(eyeL.x - eyeR.x) * 0.62;

  const push = (x: number, y: number) => {
    out.push(x, y);
  };
  // mulai dari pelipis kanan, naik ke garis rambut, turun ke pelipis kiri, lalu ikut jaw
  push(b0.x - spread * 0.06, b0.y - up * 0.72);
  push(bMid.x - spread * 0.55, bMid.y - up);
  push(bMid.x, bMid.y - up * 1.08);
  push(bMid.x + spread * 0.55, bMid.y - up);
  push(b6.x + spread * 0.06, b6.y - up * 0.72);
  push(b6.x + spread * 0.1, lerp(b6.y, get(pts, 16).y, 0.42));
  for (let i = 16; i >= 0; i--) push(pts[i * 2], pts[i * 2 + 1]);
  push(b0.x - spread * 0.1, lerp(b0.y, get(pts, 0).y, 0.42));
  // noseTip dipakai untuk menambah "kepadatan" di tengah (memperbaiki feather)
  void noseTip;
  return out.length / 2;
}


/** spline Catmull-Rom yang di-resample — untuk eyeliner / lash / brow / highlight */
export function spline(control: Pt[], samples: number, closed = false, tension = 0.5): Pt[] {
  const pts = control.slice();
  if (pts.length < 2) return pts;
  if (closed) {
    pts.unshift(pts[pts.length - 1]);
    pts.push(pts[0], pts[1]);
  } else {
    pts.unshift(pts[0]);
    pts.push(pts[pts.length - 1]);
  }
  const out: Pt[] = [];
  const seg = pts.length - 3;
  const total = samples;
  for (let s = 0; s < total; s++) {
    const g = closed ? (s / total) * seg : (s / (total - 1)) * seg;
    let i = Math.floor(g);
    if (i >= seg) i = seg - 1;
    const t = g - i;
    const p0 = pts[i],
      p1 = pts[i + 1],
      p2 = pts[i + 2],
      p3 = pts[i + 3];
    const t2 = t * t;
    const t3 = t2 * t;
    const f = (a: number, b: number, c: number, d: number) =>
      0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
    out.push({ x: f(p0.x, p1.x, p2.x, p3.x), y: f(p0.y, p1.y, p2.y, p3.y) });
    void tension;
  }
  return out;
}
