/**
 * MaskBuilder — merasterisasi region wajah (kulit / mata / bibir / gigi /
 * undereye / alis / eyeliner+lash / hidung / blush / kelopak / highlight /
 * contour) jadi geometri GPU (position + vec4 weight per vertex).
 *
 * Semua koordinat memakai posisi hasil WARP (mesh.dest) supaya mask selalu
 * menempel ke wajah yang sudah dibentuk ulang. 3 target RGBA8 = 12 kanal.
 * Additive blending dipakai di pipeline, jadi tiap primitif menulis ke kanalnya.
 */
import { IDX, centroid, clamp, eyeInfo, faceHull, get, lerp, mid, spline, type Pt } from '../face/landmarks';
import type { FaceGeom } from '../face/landmarks';
import type { FxParams } from './params';

const TARGETS = 3;
const CAP = 26000; // vertex per target (6 float masing-masing)

type PathLike = number[] | Pt[];
function flatOf(p: PathLike): number[] {
  if (p.length === 0) return [];
  if (typeof p[0] === 'number') return p as number[];
  const o: number[] = [];
  for (const q of p as Pt[]) o.push(q.x, q.y);
  return o;
}

class Writer {
  arr = new Float32Array(CAP * 6);
  n = 0;
  reset() {
    this.n = 0;
  }
  private push(x: number, y: number, ch: number, v: number) {
    if (this.n >= CAP) return;
    const o = this.n * 6;
    const a = this.arr;
    a[o] = x;
    a[o + 1] = y;
    a[o + 2] = 0;
    a[o + 3] = 0;
    a[o + 4] = 0;
    a[o + 5] = 0;
    a[o + 2 + ch] = v;
    this.n++;
  }
  /** polygon tertutup → fan dari centroid (diblur nanti, cekung tidak masalah) */
  poly(raw: PathLike, ch: number, v = 1, dilate = 1, center?: Pt) {
    const pts = flatOf(raw);
    const cnt = pts.length / 2;
    if (cnt < 3) return;
    let mx = 0,
      my = 0;
    for (let i = 0; i < cnt; i++) {
      mx += pts[i * 2];
      my += pts[i * 2 + 1];
    }
    mx /= cnt;
    my /= cnt;
    if (center) {
      mx = center.x;
      my = center.y;
    }
    for (let i = 0; i < cnt; i++) {
      const j = (i + 1) % cnt;
      const ax = mx + (pts[i * 2] - mx) * dilate;
      const ay = my + (pts[i * 2 + 1] - my) * dilate;
      const bx = mx + (pts[j * 2] - mx) * dilate;
      const by = my + (pts[j * 2 + 1] - my) * dilate;

      this.push(mx, my, ch, v);
      this.push(ax, ay, ch, v);
      this.push(bx, by, ch, v);
    }
  }
  /** pita antara dua kurva dengan bobot berbeda (gradasi lembut) */
  band(ra: PathLike, rb: PathLike, ch: number, va: number, vb: number, closed = false) {
    const a = flatOf(ra);
    const b = flatOf(rb);
    const cnt = Math.min(a.length, b.length) / 2;
    if (cnt < 2) return;
    const last = closed ? cnt : cnt - 1;
    for (let i = 0; i < last; i++) {
      const j = (i + 1) % cnt;
      this.push(a[i * 2], a[i * 2 + 1], ch, va);
      this.push(b[i * 2], b[i * 2 + 1], ch, vb);
      this.push(b[j * 2], b[j * 2 + 1], ch, vb);

      this.push(a[i * 2], a[i * 2 + 1], ch, va);
      this.push(b[j * 2], b[j * 2 + 1], ch, vb);
      this.push(a[j * 2], a[j * 2 + 1], ch, va);
    }
  }
  /** garis dengan lebar + taper (untuk liner, lash, highlight, contour) */
  stroke(raw: PathLike, ch: number, w0: number, w1: number, v = 1) {
    const pts = flatOf(raw);
    const cnt = pts.length / 2;
    if (cnt < 2) return;
    const left: number[] = [];
    const right: number[] = [];
    for (let i = 0; i < cnt; i++) {
      const t = cnt > 1 ? i / (cnt - 1) : 0;
      const a = pts[Math.max(0, i - 1) * 2];
      const ay = pts[Math.max(0, i - 1) * 2 + 1];
      const bx = pts[i * 2];
      const by = pts[i * 2 + 1];
      const cx2 = pts[Math.min(cnt - 1, i + 1) * 2];
      const cy2 = pts[Math.min(cnt - 1, i + 1) * 2 + 1];
      let dx = cx2 - a;
      let dy = cy2 - ay;
      const L = Math.hypot(dx, dy) || 1;
      dx /= L;
      dy /= L;
      const nx = -dy;
      const ny = dx;
      const w = lerp(w0, w1, t) * smoothEdge(t);
      left.push(bx + nx * w, by + ny * w);
      right.push(bx - nx * w, by - ny * w);
    }
    this.band(left, right, ch, v, v);
  }
}

function smoothEdge(t: number) {
  return Math.pow(Math.sin(clamp(t, 0, 1) * Math.PI), 0.45);
}

export interface MaskFrame {
  a: Float32Array;
  an: number;
  b: Float32Array;
  bn: number;
  c: Float32Array;
  cn: number;
  /** versi "warped geom" supaya pipeline bisa pakai lagi (hemat) */
  wgeom: FaceGeom | null;
}

export class MaskBuilder {
  private w: Writer[] = [new Writer(), new Writer(), new Writer()];
  frame: MaskFrame = { a: this.w[0].arr, an: 0, b: this.w[1].arr, bn: 0, c: this.w[2].arr, cn: 0, wgeom: null };

  get writers() {
    return this.w;
  }

  /**
   * @param dest  mesh.dest — 68 titik pertama = landmark hasil warp
   * @param g     geom ruang src (untuk metric yang tidak berubah oleh warp)
   */
  build(dest: Float32Array, g: FaceGeom, p: FxParams, warpActive: boolean): MaskFrame {
    for (const x of this.w) x.reset();
    if (g.vis <= 0.02) {
      this.frame.an = this.frame.bn = this.frame.cn = 0;
      this.frame.wgeom = null;
      return this.frame;
    }
    // pts ruang gambar setelah warp
    const pts: Float32Array = warpActive ? dest.subarray(0, 68 * 2) : g.pts;
    const wg: FaceGeom = {
      ...g,
      pts,
      eyeR: eyeInfo(pts, 'R'),
      eyeL: eyeInfo(pts, 'L'),
    };
    const A = this.w[0];
    const B = this.w[1];
    const C = this.w[2];
    const fw = Math.max(wg.width, 1e-4);
    const P = (i: number): Pt => get(pts, i);
    const flat = (list: Pt[]) => {
      const o: number[] = [];
      for (const q of list) o.push(q.x, q.y);
      return o;
    };
    const scale = (list: Pt[], k: number, c: Pt): number[] => {
      const o: number[] = [];
      for (const q of list) o.push(c.x + (q.x - c.x) * k, c.y + (q.y - c.y) * k * (k > 1 ? 1.08 : 1.05));
      return o;
    };

    /* ══ A: kulit, mata, bibir, gigi ══════════════════════════ */
    const hull: number[] = [];
    void faceHull(pts, hull);
    A.poly(hull, 0, 1);
    A.poly(hull, 0, 0.5, 1.06); // halo luar → feather lebih lebar setelah blur

    for (const eye of [wg.eyeR, wg.eyeL]) {
      const up = spline([P(eye === wg.eyeR ? 36 : 42), P(eye === wg.eyeR ? 37 : 43), P(eye === wg.eyeR ? 38 : 44), P(eye === wg.eyeR ? 39 : 45)], 12);
      const dn = spline([P(eye === wg.eyeR ? 39 : 45), P(eye === wg.eyeR ? 40 : 46), P(eye === wg.eyeR ? 41 : 47), P(eye === wg.eyeR ? 36 : 42)], 12);
      const shape = up.concat(dn.slice().reverse());
      A.poly(shape, 1, 1, 1);
      A.poly(scale(shape, 1.3, eye.c), 1, 0.4);
    }

    const lipOut = spline(ptsIdx(pts, IDX.lipOuter), 20, true);
    const lipIn = spline(ptsIdx(pts, IDX.lipInner), 16, true);
    A.poly(flat(lipOut), 2, 1, 1.04);
    A.poly(flat(lipOut), 2, 0.45, 1.35);
    A.poly(flat(lipIn), 3, 1, 0.92);

    /* ══ B: undereye, alis, liner+lash, hidung ═══════════════ */
    for (const eye of [wg.eyeR, wg.eyeL]) {
      const isR = eye === wg.eyeR;
      const lower = spline(
        [P(isR ? 39 : 45), P(isR ? 40 : 46), P(isR ? 41 : 47), P(isR ? 36 : 42)].reverse(),
        14
      );
      const inner = lower.map((q) => ({ x: q.x, y: q.y + eye.ry * 0.18 }));
      const outer = lower.map((q, i) => {
          const t = i / (lower.length - 1);
          return { x: q.x, y: q.y + eye.ry * (1.0 + 1.5 * Math.sin(t * Math.PI)) };
        });

      B.band(inner, outer, 0, 0.9, 0.0);
    }

    for (let bi = 0; bi < 2; bi++) {
      const idx = bi === 0 ? IDX.browR : IDX.browL;
      const s = spline(ptsIdx(pts, idx), 18);
      const tailTaper = (i: number) => 0.55 + 0.45 * Math.sin((i / (s.length - 1)) * Math.PI * 0.85);
      const th = Math.max(fw * 0.028, eyeH(pts) * 0.34);
      const top = s.map((q, i) => ({ x: q.x, y: q.y - th * tailTaper(i) }));
      const bot = s.map((q, i) => ({ x: q.x, y: q.y + th * 1.15 * tailTaper(i) }));
      B.band(top, bot, 1, 0.55, 0.85);
      B.band(top, bot, 1, 0.2, 0.3);
    }

    // eyeliner + bulu mata (dibangun dari spline kelopak atas)
    const linerAmt = Math.max(p.liner, p.lashes * 0.7);
    if (linerAmt > 0.001) {
      for (const eye of [wg.eyeR, wg.eyeL]) {
        const isR = eye === wg.eyeR;
        const dir = isR ? -1 : 1; // ke arah pelipis
        const upper = spline([P(isR ? 36 : 45), P(isR ? 37 : 43), P(isR ? 38 : 44), P(isR ? 39 : 45)], 18);
        const thick = fw * (0.006 + 0.020 * p.liner);
        const line: number[] = [];
        const out: number[] = [];
        for (let i = 0; i < upper.length; i++) {
          const t = i / (upper.length - 1);
          const q = upper[i];
          const nearOuter = Math.pow(clamp(isR ? 1 - t : t, 0, 1), 1.6);
          const w = thick * (0.55 + 0.85 * nearOuter);
          line.push(q.x, q.y - w * 0.35);
          out.push(q.x + dir * nearOuter * fw * 0.045 * p.liner, q.y - w * (1.0 + nearOuter * 1.2) - fw * 0.004);
        }
        B.band(line, out, 2, 0.9, 1.0);
        // bulu mata: 7 helai, taper
        const n = 7;
        for (let i = 0; i < n; i++) {
          const t = (i + 0.5) / n;
          const q = upper[Math.floor(t * (upper.length - 1))];
          const q2 = upper[Math.min(upper.length - 1, Math.floor(t * (upper.length - 1)) + 2)];
          let dx = q2.x - q.x;
          let dy = q2.y - q.y;
          const L = Math.hypot(dx, dy) || 1;
          dx /= L;
          dy /= L;
          let nx = -dy;
          let ny = dx;
          if (ny > 0) {
            nx = -nx;
            ny = -ny;
          }
          const len = fw * (0.02 + 0.055 * p.lashes) * (0.75 + 0.5 * Math.pow(Math.abs(t - 0.45) * 1.5, 1.3));
          const ex = q.x + nx * len + dir * len * 0.35;
          const ey = q.y + ny * len;
          B.stroke(
            [q.x - dx * fw * 0.006, q.y - dy * fw * 0.006, ex, ey],
            2,
            fw * 0.0075,
            fw * 0.0006,
            1
          );
        }
      }
    }

    // hidung: segitiga cup + bridge bawah ( utk contour / freckle / refine )
    const nose = [P(31), P(32), P(33), P(34), P(35)];
    const noseTip = P(33);
    B.poly(nose, 3, 0.9, 1.18, noseTip);
    B.poly(nose, 3, 0.35, 1.5, noseTip);

    /* ══ C: blush, kelopak, highlight, contour ═══════════════ */
    for (const eye of [wg.eyeR, wg.eyeL]) {
      const sgn = eye === wg.eyeR ? -1 : 1;
      const cx = eye.c.x + sgn * fw * 0.055;
      const cy = eye.c.y + fw * 0.16;
      const rot = sgn * 0.18;
      const e1: number[] = [];
      ellipse(cx, cy, fw * 0.15, fw * 0.105, rot, 24, e1);
      C.poly(e1, 0, 0.85);
      const e2: number[] = [];
      ellipse(cx, cy, fw * 0.215, fw * 0.155, rot, 24, e2);
      C.poly(e2, 0, 0.3);
    }

    for (const eye of [wg.eyeR, wg.eyeL]) {
      const isR = eye === wg.eyeR;
      const upper = spline([P(isR ? 36 : 45), P(isR ? 37 : 43), P(isR ? 38 : 44), P(isR ? 39 : 45)], 16);
      const browIdx = isR ? IDX.browR : IDX.browL;
      const brow = spline(ptsIdx(pts, browIdx), 16);
      const lid = upper.map((q) => ({ x: q.x, y: q.y - eye.ry * 0.25 }));
      const crease = brow.map((q, i) => {
          const t = i / (brow.length - 1);
          const up = lerp(eye.ry * 1.25, fw * 0.055, t * 0.4);
          return { x: q.x, y: q.y + up };
        });

      C.band(lid, crease, 1, 1.0, 0.12);
    }

    // highlighter: tulang pipi, jembatan hidung, cupid's bow, dahi, dagu
    const hi = fw;
    for (const eye of [wg.eyeR, wg.eyeL]) {
      const isR = eye === wg.eyeR;
      const sgn = isR ? -1 : 1;
      const curve: Pt[] = [];
      for (let i = 0; i < 8; i++) {
        const t = i / 7;
        curve.push({
          x: eye.c.x + sgn * lerp(0.02, 0.26, t) * hi,
          y: eye.c.y + lerp(0.42, 0.16, t) * hi + Math.sin(t * Math.PI) * hi * 0.012,
        });
      }
      C.stroke(flat(curve), 2, hi * 0.030, hi * 0.008, 0.9);
    }
    {
      const br = [P(29), P(30), P(33)];
      C.stroke(br, 2, fw * 0.018, fw * 0.012, 0.7);
      const bow = [P(51), P(52), P(53)];
      C.stroke(bow, 2, fw * 0.012, fw * 0.006, 0.55);
      const bm = mid(centroid(pts, IDX.browR), centroid(pts, IDX.browL));
      C.stroke([bm.x, bm.y - fw * 0.22, bm.x, bm.y - fw * 0.1], 2, fw * 0.045, fw * 0.02, 0.5);
    }

    // contour: siluet rahang + pelipis + sisi hidung
    for (const side of [-1, 1] as const) {
      const list = side < 0 ? [0, 1, 2, 3, 4, 5, 6] : [16, 15, 14, 13, 12, 11, 10];
      const curve = spline(ptsIdx(pts, list), 16);
      C.stroke(curve, 3, fw * (0.03 + 0.02 * p.contour), fw * 0.002, 0.85);
      const temple = [P(side < 0 ? 17 : 26), { x: P(side < 0 ? 0 : 16).x, y: P(27).y - fw * 0.05 }];
      C.stroke(temple, 3, fw * 0.026, fw * 0.004, 0.5);
    }
    {
      const nx = P(33).x;
      const top = P(30).y;
      const bot = P(33).y;
      for (const s of [-1, 1]) {
        C.stroke([nx + s * fw * 0.038, lerp(top, bot, 0.25), nx + s * fw * 0.045, bot], 3, fw * 0.013, fw * 0.003, 0.6);
      }
    }

    this.frame.a = A.arr;
    this.frame.b = B.arr;
    this.frame.c = C.arr;
    this.frame.an = A.n;
    this.frame.bn = B.n;
    this.frame.cn = C.n;
    this.frame.wgeom = wg;
    return this.frame;
  }

  readonly bytesPerVertex = 6 * 4;
  readonly targetCount = TARGETS;
}

function ptsIdx(pts: Float32Array, idx: readonly number[]): Pt[] {
  const o: Pt[] = [];
  for (const i of idx) o.push({ x: pts[i * 2], y: pts[i * 2 + 1] });
  return o;
}
function eyeH(pts: Float32Array) {
  const e = eyeInfo(pts, 'R');
  return Math.max(e.ry * 2, 1e-4);
}
function ellipse(cx: number, cy: number, rx: number, ry: number, rot: number, n: number, out: number[]) {
  for (let i = 0; i < n; i++) {
    const a = (i / n) * Math.PI * 2;
    const ca = Math.cos(a) * rx;
    const sa = Math.sin(a) * ry;
    out.push(cx + ca * Math.cos(rot) - sa * Math.sin(rot), cy + ca * Math.sin(rot) + sa * Math.cos(rot));
  }
}
