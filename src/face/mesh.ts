/**
 * FaceMesh — awan anchor (68 landmark + anchor turunan) yang di-triangulate
 * Delaunay tiap beberapa frame, lalu digeser sesuai parameter reshape.
 *
 * Ini teknik yang sama dipakai beauty cam komersial ("mesh warp / MLS"):
 * render segitiga dengan vertex position = titik hasil warp (dest) dan
 * vertex uv = titik asli (src) → GPU melakukan inverse-mapping gratis,
 * background tidak bergerak karena anchor boundary punya displacement 0.
 */
import { Delaunay } from 'd3-delaunay';
import { IDX, centroid, clamp, dist, eyeInfo, get, headPose, lerp, mid, mouthInfo, type FaceGeom } from './landmarks';
import type { FxParams } from '../fx/params';

const EYE_RING_N = 12;
const MOUTH_RING_N = 14;
const BOUND_N = 7;
const PAD = 0.06;

export class FaceMesh {
  /** jumlah anchor (fixed) */
  readonly count: number;
  base: Float32Array; // anchor src (normalized, y-down)
  dest: Float32Array; // anchor setelah warp
  /** interleaved [dx, dy, sx, sy] untuk GPU */
  verts: Float32Array;
  indices: Uint32Array;
  indexCount = 0;

  private delaunayPts = new Float64Array(0);
  private map: {
    land: number[]; // anchor idx per landmark
    eyeRing: number[][][]; // [eye][ring] → list anchor
    mouthRing: number[][]; // [ring] → list anchor
    jaw: number[];
    forehead: number[];
    nose: number[];
    cheek: number[];
    brow: number[][];
    bound: number[];
  };

  constructor() {
    const land: number[] = [];
    const jaw: number[] = [];
    const eyeRing: number[][][] = [[[], []], [[], []]];
    const mouthRing: number[][] = [[], []];
    const forehead: number[] = [];
    const nose: number[] = [];
    const cheek: number[] = [];
    const brow: number[][] = [[], []];
    const bound: number[] = [];
    let n = 0;
    for (let i = 0; i < 68; i++) land.push(n++); // 0..67 = landmark mentah
    // ring mata (radius 1.5 & 2.3) supaya pembesaran mata punya falloff lokal
    for (let e = 0; e < 2; e++)
      for (let r = 0; r < 2; r++)
        for (let s = 0; s < EYE_RING_N; s++) eyeRing[e][r].push(n++);
    // ring mulut
    for (let r = 0; r < 2; r++) for (let s = 0; s < MOUTH_RING_N; s++) mouthRing[r].push(n++);
    // titik tengah segmen jaw (memperhalus siluet)
    for (let i = 0; i < 16; i++) jaw.push(n++);
    // grid jidat 4x3
    for (let i = 0; i < 12; i++) forehead.push(n++);
    // nose cage: 8 titik
    for (let i = 0; i < 8; i++) nose.push(n++);
    // cheek anchors: 2 sisi x 4
    for (let i = 0; i < 8; i++) cheek.push(n++);
    // brow spline extra: 2 x 6
    for (let b = 0; b < 2; b++) for (let i = 0; i < 6; i++) brow[b].push(n++);
    // boundary grid
    for (let i = 0; i < BOUND_N * BOUND_N; i++) bound.push(n++);

    this.count = n;
    this.base = new Float32Array(n * 2);
    this.dest = new Float32Array(n * 2);
    this.verts = new Float32Array(n * 4);
    this.delaunayPts = new Float64Array(n * 2);
    this.indices = new Uint32Array(n * 12);
    this.map = { land, eyeRing, mouthRing, jaw, forehead, nose, cheek, brow, bound };
  }

  /** hitung posisi anchor src dari landmark + geometry */
  layout(g: FaceGeom) {
    const pts = g.pts;
    const m = this.map;
    const a = (i: number, x: number, y: number) => {
      this.base[i * 2] = x;
      this.base[i * 2 + 1] = y;
    };
    for (let i = 0; i < 68; i++) a(m.land[i], pts[i * 2], pts[i * 2 + 1]);

    const eyes = [g.eyeR, g.eyeL];
    for (let e = 0; e < 2; e++) {
      const eye = eyes[e];
      for (let r = 0; r < 2; r++) {
        const k = r === 0 ? 1.5 : 2.35;
        const list = m.eyeRing[e][r];
        for (let s = 0; s < list.length; s++) {
          const ang = (s / list.length) * Math.PI * 2;
          a(list[s], eye.c.x + Math.cos(ang) * eye.rx * k, eye.c.y + Math.sin(ang) * eye.ry * k * 1.15);
        }
      }
    }
    for (let r = 0; r < 2; r++) {
      const k = r === 0 ? 1.45 : 2.1;
      const list = m.mouthRing[r];
      for (let s = 0; s < list.length; s++) {
        const ang = (s / list.length) * Math.PI * 2;
        a(list[s], g.mouth.c.x + Math.cos(ang) * g.mouth.rx * k, g.mouth.c.y + Math.sin(ang) * g.mouth.ry * k * 1.25);
      }
    }
    for (let i = 0; i < 16; i++) a(m.jaw[i], lerp(pts[i * 2], pts[(i + 1) * 2], 0.5), lerp(pts[i * 2 + 1], pts[(i + 1) * 2 + 1], 0.5));

    const bMid = mid(centroid(pts, IDX.browR), centroid(pts, IDX.browL));
    const eyeMid = mid(g.eyeR.c, g.eyeL.c);
    const faceH = Math.max(Math.abs(get(pts, 8).y - bMid.y), 1e-4);
    const hy = bMid.y - faceH * 1.02; // estimasi garis rambut
    for (let i = 0; i < 12; i++) {
      const row = Math.floor(i / 4);
      const col = i % 4;
      const t = row / 2;
      const y = lerp(bMid.y - faceH * 0.12, hy, t);
      const spread = lerp(Math.abs(g.eyeL.c.x - g.eyeR.c.x) * 1.15, Math.abs(g.eyeL.c.x - g.eyeR.c.x) * 0.75, t);
      a(m.forehead[i], eyeMid.x + (col / 3 - 0.5) * spread, y);
    }
    // nose cage: sayap + titik antara bridge & base
    const nb = centroid(pts, IDX.noseBase);
    const ntip = get(pts, 33);
    const nbridge = [get(pts, 29), get(pts, 30)];
    const ncx = nbridge[1].x;
    const cage: [number, number][] = [
      [lerp(ncx, get(pts, 31).x, 1.25), lerp(ntip.y, nb.y, 0.45)],
      [lerp(ncx, get(pts, 35).x, 1.25), lerp(ntip.y, nb.y, 0.45)],
      [ncx, lerp(nbridge[1].y, ntip.y, 0.5)],
      [ncx, ntip.y],
      [lerp(ncx, get(pts, 32).x, 1.6), nb.y],
      [lerp(ncx, get(pts, 34).x, 1.6), nb.y],
      [ncx, lerp(nb.y, g.mouth.c.y, 0.28)],
      [ncx, lerp(nbridge[0].y, nb.y, 0.3)],
    ];
    for (let i = 0; i < 8; i++) a(m.nose[i], cage[i][0], cage[i][1]);

    // cheek anchors: di bawah mata, arah pelipis (untuk contour & cheekbone)
    for (let e = 0; e < 2; e++) {
      const sgn = e === 0 ? -1 : 1;
      const eye = eyes[e];
      for (let i = 0; i < 4; i++) {
        const t = (i + 1) / 4;
        a(
          m.cheek[e * 4 + i],
          eye.c.x + sgn * lerp(0.15, 0.62, t) * g.width,
          lerp(eye.c.y + eye.ry * 1.7, g.mouth.c.y - faceH * 0.16, t * 0.8)
        );
      }
    }
    for (let b = 0; b < 2; b++) {
      const list = b === 0 ? IDX.browR : IDX.browL;
      for (let i = 0; i < 6; i++) {
        const t = i / 5;
        const idx0 = list[Math.min(4, Math.floor(t * 4))];
        const p = get(pts, idx0);
        a(m.brow[b][i], p.x, p.y - faceH * (0.02 + 0.05 * t));
      }
    }
    // boundary grid (termasuk luar frame biar warp tidak narik tepi gambar)
    for (let i = 0; i < BOUND_N * BOUND_N; i++) {
      const cx = i % BOUND_N;
      const cy = (i / BOUND_N) | 0;
      let x = cx / (BOUND_N - 1);
      let y = cy / (BOUND_N - 1);
      x = -PAD + x * (1 + PAD * 2);
      y = -PAD + y * (1 + PAD * 2);
      a(m.bound[i], x, y);
    }
  }

  /** rebuild triangulasi (dipanggil lebih jarang dari layout) */
  triangulate() {
    const dp = this.delaunayPts;
    for (let i = 0; i < this.count * 2; i++) dp[i] = this.base[i];
    try {
      const del = new Delaunay(dp);
      const tri = del.triangles as Uint32Array;
      this.indexCount = Math.min(tri.length, this.indices.length);
      this.indices.set(tri.subarray(0, this.indexCount));
    } catch {
      this.indexCount = 0;
    }
    return this.indexCount;
  }

  /** terapkan semua displacement reshape → this.dest + this.verts */
  displace(g: FaceGeom, p: FxParams, aspect: number, strength: number) {
    const base = this.base;
    const dest = this.dest;
    for (let i = 0; i < this.count * 2; i++) dest[i] = base[i];

    if (g.vis <= 0.01 || strength <= 0.001) {
      this.flushVerts();
      return;
    }
    // guard: profil samping bikin warp aneh → kurangi
    const guard = g.vis * strength * (1 - smoothstep(0.42, 0.92, Math.abs(g.yaw)));
    const eyeMid = mid(g.eyeR.c, g.eyeL.c);
    const cx = eyeMid.x;
    const pts = g.pts;
    const browY = mid(centroid(pts, IDX.browR), centroid(pts, IDX.browL)).y;
    const chinY = get(pts, 8).y;
    const fh = Math.max(chinY - browY, 1e-4);
    const fw = Math.max(g.width, 1e-4);
    const mouthY = g.mouth.c.y;

    const dx = new Float32Array(this.count);
    const dy = new Float32Array(this.count);

    // gerbang kotak wajah: anchor di luar wajah (background / boundary grid) tidak
    // boleh bergeser sama sekali — kalau boleh, background ikut "melar" (artefak khas
    // beauty cam murahan).
    const yTop = browY - fh * 1.45;
    const yBot = chinY + fh * 0.42;
    const halfW = fw * 0.82 * aspect;
    const padX = Math.max(fw * 0.5 * aspect, 1e-4);
    const padY = Math.max(fh * 0.35, 1e-4);

    const jawLeftX = (y: number) => this.jawSilhouette(pts, y, -1);
    const jawRightX = (y: number) => this.jawSilhouette(pts, y, +1);

    const noseX = (y: number) => {
      const b = get(pts, 30);
      const t = get(pts, 33);
      const k = clamp((y - b.y) / Math.max(t.y - b.y, 1e-4), 0, 1);
      return lerp(b.x, t.x, k);
    };

    for (let i = 0; i < this.count; i++) {
      const x = base[i * 2];
      const y = base[i * 2 + 1];
      const ux = (x - cx) * aspect; // ruang isotropik
      let ox = 0;
      let oy = 0;

      /* ── V-line / jaw slim ─────────────────────────────── */
      if (p.jawSlim !== 0) {
        const t = clamp((y - eyeMid.y) / Math.max(chinY - eyeMid.y, 1e-4), 0, 1);
        const vt = Math.pow(smoothstep(0.06, 1, t), 1.15);
        const jl = jawLeftX(y);
        const jr = jawRightX(y);
        if (jl !== null && jr !== null && t > 0.02) {
          const halfW = Math.max((jr - jl) * 0.5, 1e-4);
            const mx = (jr + jl) * 0.5;
          const rel = clamp((x - mx) / halfW, -1, 1); // -1..1 posisi relatif siluet
          const near = Math.pow(Math.abs(rel), 1.7);
          const pull = p.jawSlim * 0.075 * fw * vt * near;
          ox -= Math.sign(rel) * pull;
        }
      }

      /* ── face taper (pelipis) ──────────────────────────── */
      if (p.earFade > 0) {
        const t = clamp((browY - y) / Math.max(fh * 1.0, 1e-4), 0, 1);
        const bandY = Math.exp(-Math.pow((y - (browY - fh * 0.5)) / (fh * 0.42), 2));
        const near = clamp(Math.abs(ux) / (fw * 0.6 * aspect) - 0.45, 0, 1);
        ox -= Math.sign(ux) * p.earFade * 0.045 * fw * Math.pow(t, 0.9) * Math.pow(near, 1.2) * bandY;
      }

      /* ── cheekbone lift ───────────────────────────────── */
      if (p.cheek > 0) {
        const dy2 = y - (mouthY - fh * 0.16);
        const dxa = Math.abs(ux) - fw * 0.34 * aspect;
        const d = Math.hypot(dxa, dy2 * 1.5) / (fh * 0.6);
        const w = Math.exp(-d * d * 1.9);
        oy -= p.cheek * 0.03 * fh * w;
        ox -= Math.sign(ux) * p.cheek * 0.016 * fw * w;
      }

      /* ── panjang wajah (dagu) ─────────────────────────── */
      if (p.chin !== 0) {
        const t = clamp((y - mouthY) / Math.max(chinY - mouthY, 1e-4), 0, 1);
        if (t > 0) {
          const wx = Math.exp(-Math.pow(ux / Math.max(fw * 0.6 * aspect, 1e-4), 2));
          oy += p.chin * 0.055 * fh * Math.pow(t, 1.2) * wx;
        }
      }
      /* ── tinggi jidat ─────────────────────────────────── */
      if (p.forehead !== 0) {
        const t = clamp((browY - y) / Math.max(fh * 0.9, 1e-4), 0, 1);
        if (t > 0) {
          const wx = Math.exp(-Math.pow(ux / Math.max(fw * 0.66 * aspect, 1e-4), 2));
          oy += p.forehead * 0.07 * fh * Math.pow(t, 1.1) * wx;
        }
      }

      /* ── mata ─────────────────────────────────────────── */
      for (let e = 0; e < 2; e++) {
        const eye = e === 0 ? g.eyeR : g.eyeL;
        const dxr = (x - eye.c.x) * aspect;
        const dyr = y - eye.c.y;
        const d = Math.hypot(dxr / (eye.rx * 2 * aspect), dyr / (eye.ry * 2.3));
        if (d < 1.9) {
          const w = Math.exp(-d * d * 2.1);
          if (p.eyeEnlarge > 0) {
            ox += (x - eye.c.x) * p.eyeEnlarge * 0.3 * w;
            oy += (y - eye.c.y) * p.eyeEnlarge * 0.3 * w;
          }
          if (p.eyeSpacing !== 0) ox += (e === 0 ? -1 : 1) * p.eyeSpacing * 0.05 * fw * w;
          if (p.browLift > 0 && y < eye.c.y) {
            const wb = Math.exp(-Math.pow((y - (eye.c.y - eye.ry * 1.8)) / (fh * 0.09), 2));
            oy -= p.browLift * 0.035 * fh * wb;
          }
        }
      }

      /* ── hidung ───────────────────────────────────────── */
      if (p.noseSlim > 0 || p.noseShort !== 0) {
        const nx = noseX(y);
        const dxa = Math.abs(ux - (nx - cx) * aspect);
        const inY = clamp((y - get(pts, 30).y) / Math.max(get(pts, 33).y - get(pts, 30).y, 1e-4), -0.2, 1.35);
        const fallY = Math.exp(-Math.pow((inY - 0.72) * 1.7, 2));
        const fallX = Math.exp(-Math.pow(dxa / (fw * 0.16 * aspect), 2));
        const w = fallX * fallY;
        ox -= Math.sign(ux - (nx - cx) * aspect) * p.noseSlim * 0.045 * fw * w;
        if (p.noseShort !== 0 && inY > 0) {
          oy += p.noseShort * 0.05 * fh * clamp(inY, 0, 1) * fallY;
        }
      }

      /* ── bibir / senyum ───────────────────────────────── */
      {
        const dxm = (x - g.mouth.c.x) * aspect;
        const dym = y - g.mouth.c.y;
        const d = Math.hypot(dxm / Math.max(g.mouth.rx * 2.2 * aspect, 1e-4), dym / Math.max(g.mouth.ry * 3.0, 1e-4));
        if (d < 1.9) {
          const w = Math.exp(-d * d * 1.8);
          if (p.lipPlump > 0) {
            ox += (x - g.mouth.c.x) * p.lipPlump * 0.12 * w;
            oy += (y - g.mouth.c.y) * p.lipPlump * 0.34 * w;
          }
          const smile = p.smile * (0.35 + 0.65 * g.happy);
          if (smile > 0.001) {
            const cornerL = get(pts, 48);
            const cornerR = get(pts, 54);
            const dl = dist({ x: x, y: y }, cornerL);
            const dr = dist({ x: x, y: y }, cornerR);
            const wc = Math.exp(-Math.pow(dl / (fw * 0.28), 2)) + Math.exp(-Math.pow(dr / (fw * 0.28), 2));
            oy -= smile * 0.045 * fh * wc;
            ox += Math.sign(dxm || 1) * smile * 0.02 * fw * wc;
          }
        }
      }

      const ex = Math.max(0, Math.abs(ux) - halfW) / padX;
      const ey = Math.max(0, Math.max(yTop - y, y - yBot)) / padY;
      const gate = Math.exp(-(ex * ex + ey * ey) * 2.4);
      dx[i] = ox * guard * gate;
      dy[i] = oy * guard * gate;
    }

    for (let i = 0; i < this.count; i++) {
      dest[i * 2] = base[i * 2] + dx[i];
      dest[i * 2 + 1] = base[i * 2 + 1] + dy[i];
    }
    this.flushVerts();
  }

  private flushVerts() {
    const v = this.verts;
    const b = this.base;
    const d = this.dest;
    for (let i = 0; i < this.count; i++) {
      v[i * 4] = d[i * 2];
      v[i * 4 + 1] = d[i * 2 + 1];
      v[i * 4 + 2] = b[i * 2];
      v[i * 4 + 3] = b[i * 2 + 1];
    }
  }

  /** interpolasi siluet jaw pada ketinggian y (side=-1 kiri, +1 kanan) */
  private jawSilhouette(pts: Float32Array, y: number, side: number): number | null {
    const jaw = IDX.jaw;
    const list = side < 0 ? jaw.slice(0, 9) : jaw.slice(8).reverse();
    let best: number | null = null;
    let bestD = Infinity;
    for (let i = 0; i < list.length - 1; i++) {
      const a = get(pts, list[i]);
      const b = get(pts, list[i + 1]);
      const lo = Math.min(a.y, b.y);
      const hi = Math.max(a.y, b.y);
      if (y >= lo - 1e-4 && y <= hi + 1e-4) {
        const t = hi - lo < 1e-5 ? 0 : (y - a.y) / (hi - lo);
        return lerp(a.x, b.x, clamp(t, 0, 1));
      }
      const d = Math.abs(a.y - y);
      if (d < bestD) {
        bestD = d;
        best = a.x;
      }
    }
    return bestD < 0.05 ? best : null;
  }
}

export function smoothstep(a: number, b: number, x: number) {
  const t = clamp((x - a) / Math.max(b - a, 1e-6), 0, 1);
  return t * t * (3 - 2 * t);
}

/** bangun FaceGeom dari landmark mentah (normalized, y-down) */
export function buildGeom(
  pts: Float32Array,
  vis: number,
  box: [number, number, number, number],
  expr: { happy: number; surprised: number; neutral: number }
): FaceGeom {
  const er = eyeInfo(pts, 'R');
  const el = eyeInfo(pts, 'L');
  const mouth = mouthInfo(pts);
  const jawW = dist(get(pts, 0), get(pts, 16));
  const width = Math.max(jawW * 0.86 + Math.abs(el.c.x - er.c.x) * 0.22, 1e-4);
  const pose = headPose(pts, width);
  return {
    pts,
    vis,
    box,
    center: { x: mouth.c.x * 0.35 + er.c.x * 0.325 + el.c.x * 0.325, y: (er.c.y + el.c.y + mouth.c.y * 2) / 4 },
    width,
    roll: pose.roll,
    yaw: pose.yaw,
    pitch: pose.pitch,
    eyeR: er,
    eyeL: el,
    mouth,
    happy: clamp(expr.happy, 0, 1),
    surprised: clamp(expr.surprised, 0, 1),
    // eye aspect ratio: terbuka ~0.30, tertutup ~0.13
    blink: clamp((0.26 - (er.ry / Math.max(er.rx, 1e-4) + el.ry / Math.max(el.rx, 1e-4)) * 0.5) / 0.13, 0, 1),
  };
}
