/**
 * OverlayRenderer — lapisan AR (lens) + frame, digambar dengan Canvas2D ke satu
 * canvas seukuran output lalu di-upload jadi texture di pass `present`.
 *
 * Kenapa Canvas2D dan bukan mesh 3D: semua lens di HaoCam bersifat 2D-anchored
 * (ikut roll/scale/posisi kepala dari face mesh) — sama seperti mayoritas lens
 * "beauty sticker" di app komersial, dan bisa dibuat prosedural sehingga repo
 * ini tidak butuh aset biner sama sekali.
 */
import { clamp, get, mid, type FaceGeom } from '../face/landmarks';
import { LENSES, FRAMES, type FxParams } from './params';

export interface OverlayState {
  geom: FaceGeom | null;
  params: FxParams;
  time: number;
  /** 0..1 trigger hasil deteksi ekspresi (senyum / buka mulut / kedip) */
  smile: number;
  openMouth: number;
  blink: number;
  showMesh?: boolean;
}

export class OverlayRenderer {
  readonly canvas = document.createElement('canvas');
  private ctx = this.canvas.getContext('2d', { alpha: true })!;
  private W = 0;
  private H = 0;

  constructor() {
    this.canvas.width = 2;
    this.canvas.height = 2;
  }

  resize(w: number, h: number) {
    if (this.W === w && this.H === h) return;
    this.W = this.canvas.width = Math.max(2, w);
    this.H = this.canvas.height = Math.max(2, h);
  }

  draw(s: OverlayState) {
    const ctx = this.ctx;
    if (!this.W) return;
    ctx.clearRect(0, 0, this.W, this.H);
    if (s.params.frame > 0 && s.params.frame < FRAMES.length) {
      this.drawFrame(ctx, FRAMES[s.params.frame].id, s);
    }
    if (s.showMesh && s.geom) this.meshDebug(ctx, s.geom);
    if (s.geom && s.geom.vis > 0.15 && s.params.lens >= 0 && s.params.lens < LENSES.length) {
      const id = LENSES[s.params.lens].id;
      if (id !== 'none') {
        ctx.save();
        this.lens(ctx, id, s);
        ctx.restore();
      }
    }
  }

  /* ── koordinat helper (face space → px) ─────────────────── */
  private px(x: number, y: number): [number, number] {
    return [x * this.W, y * this.H];
  }
  private faceScale(g: FaceGeom) {
    return Math.max(g.width * this.W, 1);
  }

  private lens(ctx: CanvasRenderingContext2D, id: string, s: OverlayState) {
    const g = s.geom!;
    const k = clamp(s.params.lensIntensity, 0, 1);
    const F = this.faceScale(g);
    const cx = g.center.x * this.W;
    const cy = g.center.y * this.H;
    ctx.globalAlpha = k;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    const brow = this.px(...ptPair(mid(get(g.pts, 21), get(g.pts, 22))));
    const top = this.px(...ptPair(hairline(g)));
    void cy;
    switch (id) {
      case 'cat':
        this.ears(ctx, g, F, top, '#f7f2ee', '#ffd9e2');
        this.catNose(ctx, g, F, s);
        this.whiskers(ctx, g, F, '#f7f2ee');
        break;
      case 'bunny':
        this.bunnyEars(ctx, F, top, s);
        this.catNose(ctx, g, F, s, '#ffb3c1');
        break;
      case 'whiskers':
        this.roundEars(ctx, g, F, top);
        this.whiskers(ctx, g, F, '#cfd6e6');
        this.catNose(ctx, g, F, s, '#f6a5b8');
        break;
      case 'halo':
        this.halo(ctx, cx, top[1], F, s.time);
        break;
      case 'crown':
        this.crown(ctx, top[0], top[1], F, s.time);
        break;
      case 'hearts':
        this.hearts(ctx, g, F, s);
        break;
      case 'shades':
        this.shades(ctx, g, F, s);
        break;
      case 'glitter':
        this.glitter(ctx, g, F, s, brow);
        break;
      case 'tears':
        this.tears(ctx, g, F, s);
        break;
      case 'mecha':
        this.mecha(ctx, g, F, s);
        break;
      case 'bubble':
        this.bubble(ctx, g, F, s);
        break;
      default:
        break;
    }
  }

  /* ── lens primitives ─────────────────────────────────────── */
  private ears(ctx: CanvasRenderingContext2D, _g: FaceGeom, F: number, top: [number, number], fill: string, inner: string) {
    void _g;
    const [tx, ty] = top;
    const wig = Math.sin(performance.now() / 420) * 0.05;
    for (const side of [-1, 1] as const) {
      ctx.save();
      ctx.translate(tx + side * F * 0.28, ty + F * 0.04);
      ctx.rotate(side * (0.16 + wig * side));
      const w = F * 0.24;
      const h = F * 0.34;
      ctx.beginPath();
      ctx.moveTo(-w * 0.5, 0);
      ctx.quadraticCurveTo(-w * 0.1, -h, w * 0.02, -h * 0.98);
      ctx.quadraticCurveTo(w * 0.4, -h * 0.5, w * 0.55, 0);
      ctx.closePath();
      const grd = ctx.createLinearGradient(0, -h, 0, 0);
      grd.addColorStop(0, '#ffffff');
      grd.addColorStop(1, fill);
      ctx.fillStyle = grd;
      ctx.shadowColor = 'rgba(0,0,0,.28)';
      ctx.shadowBlur = F * 0.05;
      ctx.fill();
      ctx.shadowBlur = 0;
      ctx.beginPath();
      ctx.moveTo(-w * 0.22, -h * 0.05);
      ctx.quadraticCurveTo(0, -h * 0.72, w * 0.05, -h * 0.68);
      ctx.quadraticCurveTo(w * 0.2, -h * 0.32, w * 0.26, -h * 0.05);
      ctx.closePath();
      ctx.fillStyle = inner;
      ctx.fill();
      ctx.restore();
    }
  }
  private roundEars(ctx: CanvasRenderingContext2D, _g: FaceGeom, F: number, top: [number, number]) {
    void _g;
    const [tx, ty] = top;
    for (const side of [-1, 1] as const) {
      ctx.beginPath();
      ctx.arc(tx + side * F * 0.4, ty + F * 0.02, F * 0.19, 0, Math.PI * 2);
      ctx.fillStyle = '#dfe4f2';
      ctx.fill();
      ctx.beginPath();
      ctx.arc(tx + side * F * 0.4, ty + F * 0.03, F * 0.11, 0, Math.PI * 2);
      ctx.fillStyle = '#ffc2d1';
      ctx.fill();
    }
  }
  private bunnyEars(ctx: CanvasRenderingContext2D, F: number, top: [number, number], s: OverlayState) {
    const [tx, ty] = top;
    const sway = Math.sin(s.time * 1.6) * 0.09 + s.smile * 0.1;
    for (const side of [-1, 1] as const) {
      ctx.save();
      ctx.translate(tx + side * F * 0.2, ty + F * 0.03);
      ctx.rotate(side * (0.3 + sway * side));
      const w = F * 0.14;
      const h = F * 0.72;
      ctx.beginPath();
      ctx.ellipse(0, -h * 0.5, w, h * 0.5, 0, 0, Math.PI * 2);
      const grd = ctx.createLinearGradient(0, -h, 0, 0);
      grd.addColorStop(0, '#ffffff');
      grd.addColorStop(1, '#ffeef4');
      ctx.fillStyle = grd;
      ctx.shadowColor = 'rgba(0,0,0,.25)';
      ctx.shadowBlur = F * 0.06;
      ctx.fill();
      ctx.shadowBlur = 0;
      ctx.beginPath();
      ctx.ellipse(0, -h * 0.48, w * 0.45, h * 0.36, 0, 0, Math.PI * 2);
      ctx.fillStyle = '#ffc9d8';
      ctx.fill();
      ctx.restore();
    }
  }
  private catNose(ctx: CanvasRenderingContext2D, g: FaceGeom, F: number, _s: OverlayState, color = '#ff9fb2') {
    void _s;
    const [nx, ny] = this.px(...ptPair(get(g.pts, 33)));
    const r = F * 0.075;
    ctx.beginPath();
    ctx.moveTo(nx, ny + r * 0.7);
    ctx.lineTo(nx - r, ny - r * 0.55);
    ctx.quadraticCurveTo(nx, ny - r * 1.1, nx + r, ny - r * 0.55);
    ctx.closePath();
    ctx.fillStyle = color;
    ctx.shadowColor = 'rgba(0,0,0,.3)';
    ctx.shadowBlur = F * 0.03;
    ctx.fill();
    ctx.shadowBlur = 0;
  }
  private whiskers(ctx: CanvasRenderingContext2D, g: FaceGeom, F: number, color: string) {
    const [nx, ny] = this.px(...ptPair(get(g.pts, 33)));

    ctx.strokeStyle = color;
    ctx.lineWidth = Math.max(1.4, F * 0.012);
    ctx.shadowColor = 'rgba(0,0,0,.2)';
    ctx.shadowBlur = F * 0.02;
    for (const side of [-1, 1] as const)
      for (let i = 0; i < 3; i++) {
        const dy = (i - 1) * F * 0.055;
        ctx.beginPath();
        ctx.moveTo(nx + side * F * 0.11, ny + dy * 0.4);
        ctx.quadraticCurveTo(nx + side * F * 0.3, ny + dy, nx + side * F * 0.52, ny + dy * 1.6);
        ctx.stroke();
      }
    ctx.shadowBlur = 0;
  }
  private halo(ctx: CanvasRenderingContext2D, cx: number, ty: number, F: number, t: number) {
    ctx.save();
    ctx.translate(cx, ty - F * 0.16);
    ctx.rotate(Math.sin(t * 0.7) * 0.12);
    const rx = F * 0.34;
    const ry = F * 0.09;
    for (let i = 0; i < 3; i++) {
      ctx.beginPath();
      ctx.ellipse(0, 0, rx, ry, 0, 0, Math.PI * 2);
      ctx.strokeStyle = `rgba(255,${214 - i * 30},${110 + i * 40},${0.85 - i * 0.24})`;
      ctx.lineWidth = F * (0.05 - i * 0.012);
      ctx.shadowColor = 'rgba(255,205,90,.85)';
      ctx.shadowBlur = F * 0.16;
      ctx.stroke();
    }
    ctx.shadowBlur = 0;
    for (let i = 0; i < 10; i++) {
      const a = t * 0.9 + (i / 10) * Math.PI * 2;
      const x = Math.cos(a) * rx * 1.25;
      const y = Math.sin(a) * ry * 2.6 - F * 0.02;
      const s = (0.5 + 0.5 * Math.sin(t * 3 + i)) * F * 0.03;
      star(ctx, x, y, s, 'rgba(255,246,210,.9)');
    }
    ctx.restore();
  }
  private crown(ctx: CanvasRenderingContext2D, cx: number, ty: number, F: number, t: number) {
    ctx.save();
    ctx.translate(cx, ty - F * 0.02 + Math.sin(t * 1.8) * F * 0.012);
    ctx.rotate(Math.sin(t * 0.9) * 0.04);
    const w = F * 0.5;
    const h = F * 0.26;
    ctx.beginPath();
    ctx.moveTo(-w / 2, 0);
    ctx.lineTo(-w / 2, -h * 0.45);
    ctx.lineTo(-w * 0.25, -h * 0.05);
    ctx.lineTo(0, -h);
    ctx.lineTo(w * 0.25, -h * 0.05);
    ctx.lineTo(w / 2, -h * 0.45);
    ctx.lineTo(w / 2, 0);
    ctx.closePath();
    const grd = ctx.createLinearGradient(0, -h, 0, 0);
    grd.addColorStop(0, '#fff3b0');
    grd.addColorStop(0.45, '#ffd34c');
    grd.addColorStop(1, '#e09a12');
    ctx.fillStyle = grd;
    ctx.shadowColor = 'rgba(255,190,60,.55)';
    ctx.shadowBlur = F * 0.12;
    ctx.fill();
    ctx.shadowBlur = 0;
    ctx.strokeStyle = 'rgba(120,72,0,.5)';
    ctx.lineWidth = F * 0.012;
    ctx.stroke();
    for (let i = 0; i < 3; i++) {
      const x = (i - 1) * w * 0.3;
      ctx.beginPath();
      ctx.arc(x, -h * (i === 1 ? 1.02 : 0.5), F * 0.03, 0, Math.PI * 2);
      ctx.fillStyle = ['#ff5f8f', '#7ad7ff', '#a4ff8f'][i];
      ctx.fill();
    }
    ctx.restore();
  }
  private hearts(ctx: CanvasRenderingContext2D, g: FaceGeom, F: number, s: OverlayState) {
    const beat = 1 + Math.sin(s.time * 6.2) * 0.07 + s.smile * 0.12;
    for (const eye of [g.eyeR, g.eyeL]) {
      const [x, y] = this.px(eye.c.x, eye.c.y);
      const r = Math.max(eye.rx * this.W, F * 0.11) * 1.5 * beat;
      heart(ctx, x, y - r * 0.06, r, '#ff4d79', true);
      heart(ctx, x, y - r * 0.06, r * 0.62, 'rgba(255,190,205,.55)', false);
    }
  }
  private shades(ctx: CanvasRenderingContext2D, g: FaceGeom, F: number, s: OverlayState) {
    const er = this.px(g.eyeR.c.x, g.eyeR.c.y);
    const el = this.px(g.eyeL.c.x, g.eyeL.c.y);
    const w = Math.abs(el[0] - er[0]) * 0.72;
    const h = w * 0.62;
    ctx.save();
    ctx.translate((er[0] + el[0]) / 2, (er[1] + el[1]) / 2);
    ctx.rotate(g.roll);
    ctx.translate(-(er[0] + el[0]) / 2, -(er[1] + el[1]) / 2);
    const tilt = -0.06 - s.smile * 0.05;
    for (const [x] of [er, el]) {
      ctx.save();
      ctx.translate(x, (er[1] + el[1]) / 2);
      ctx.rotate(tilt);
      const r = Math.min(w, h) * 0.35;
      roundRect(ctx, -w / 2, -h / 2, w, h, r);
      const grd = ctx.createLinearGradient(0, -h / 2, 0, h / 2);
      grd.addColorStop(0, 'rgba(12,14,22,.96)');
      grd.addColorStop(0.55, 'rgba(28,32,48,.92)');
      grd.addColorStop(1, 'rgba(60,52,90,.85)');
      ctx.fillStyle = grd;
      ctx.shadowColor = 'rgba(0,0,0,.45)';
      ctx.shadowBlur = F * 0.05;
      ctx.fill();
      ctx.shadowBlur = 0;
      ctx.lineWidth = F * 0.016;
      ctx.strokeStyle = 'rgba(250,250,255,.22)';
      ctx.stroke();
      // refleksi diagonal ala lensa glossy
      ctx.save();
      roundRect(ctx, -w / 2, -h / 2, w, h, r);
      ctx.clip();
      ctx.globalAlpha = 0.5;
      ctx.fillStyle = 'rgba(255,255,255,.85)';
      ctx.beginPath();
      ctx.moveTo(-w * 0.45, h * 0.5);
      ctx.lineTo(-w * 0.05, -h * 0.5);
      ctx.lineTo(w * 0.08, -h * 0.5);
      ctx.lineTo(-w * 0.32, h * 0.5);
      ctx.closePath();
      ctx.fill();
      ctx.restore();
      ctx.restore();
    }
    // bridge + temples
    ctx.strokeStyle = 'rgba(18,20,28,.95)';
    ctx.lineWidth = F * 0.022;
    ctx.beginPath();
    ctx.moveTo(er[0] + w * 0.42, (er[1] + el[1]) / 2 - h * 0.18);
    ctx.quadraticCurveTo((er[0] + el[0]) / 2, (er[1] + el[1]) / 2 - h * 0.5, el[0] - w * 0.42, (er[1] + el[1]) / 2 - h * 0.18);
    ctx.stroke();
    ctx.restore();
  }
  private glitter(ctx: CanvasRenderingContext2D, g: FaceGeom, F: number, s: OverlayState, _brow: [number, number]) {
    void _brow;
    const seed = Math.floor(s.time * 8);
    ctx.save();
    for (let i = 0; i < 44; i++) {
      const a = hash(seed + i * 977) * Math.PI * 2;
      const rr = Math.pow(hash(seed * 3 + i * 31), 0.6);
      const x = (g.center.x + Math.cos(a) * rr * 0.24) * this.W;
      const y = (g.center.y + Math.sin(a) * rr * 0.3) * this.H;
      const life = (s.time * 1.6 + hash(i * 13) * 10) % 1;
      const sz = F * 0.028 * (0.5 + hash(i * 7)) * Math.sin(life * Math.PI);
      const hue = hash(i * 3) > 0.5 ? 'rgba(255,236,180,' : 'rgba(200,230,255,';
      star(ctx, x, y, sz, hue + (0.35 + 0.65 * Math.sin(life * Math.PI)).toFixed(2) + ')');
    }
    ctx.restore();
  }
  private tears(ctx: CanvasRenderingContext2D, g: FaceGeom, F: number, s: OverlayState) {
    for (const eye of [g.eyeR, g.eyeL]) {
      for (let i = 0; i < 3; i++) {
        const life = ((s.time * 0.42 + i * 0.33) % 1);
        const x = (eye.c.x + (i - 1) * eye.rx * 0.5) * this.W;
        const y = (eye.c.y + eye.ry * 1.1 + life * 0.16) * this.H;
        const sz = F * 0.05 * (1 - life * 0.35);
        ctx.globalAlpha = (1 - life) * clamp(s.params.lensIntensity, 0, 1);
        star4(ctx, x, y, sz, 'rgba(150,215,255,.95)');
        star4(ctx, x, y, sz * 0.45, 'rgba(255,255,255,.9)');
      }
      ctx.globalAlpha = 1;
    }
  }
  private mecha(ctx: CanvasRenderingContext2D, g: FaceGeom, F: number, s: OverlayState) {
    const [x0, y0] = [ (g.box[0]) * this.W, g.box[1] * this.H ];
    const [bw, bh] = [g.box[2] * this.W * 1.5, g.box[3] * this.H * 1.45];
    const cx = x0 + bw / 2;
    const cy = y0 + bh / 2;
    ctx.save();
    ctx.lineWidth = Math.max(1, F * 0.006);
    const stroke = (a: string) => {
      ctx.strokeStyle = a;
      ctx.stroke();
    };
    // corner brackets
    const cl = F * 0.1;
    for (const sx of [-1, 1])
      for (const sy of [-1, 1]) {
        ctx.beginPath();
        const x = cx + (sx * bw) / 2;
        const y = cy + (sy * bh) / 2;
        ctx.moveTo(x - sx * cl, y);
        ctx.lineTo(x, y);
        ctx.lineTo(x, y - sy * cl);
        stroke('rgba(120,235,255,.85)');
      }
    // scan line
    const sy = y0 + ((s.time * 0.35) % 1) * bh;
    const grd = ctx.createLinearGradient(0, sy - F * 0.06, 0, sy + F * 0.06);
    grd.addColorStop(0, 'rgba(120,235,255,0)');
    grd.addColorStop(0.5, 'rgba(120,235,255,.42)');
    grd.addColorStop(1, 'rgba(120,235,255,0)');
    ctx.fillStyle = grd;
    ctx.fillRect(x0, sy - F * 0.06, bw, F * 0.12);
    // reticle + data
    ctx.save();
    ctx.translate(cx, cy);
    ctx.rotate(s.time * 0.6);
    ctx.beginPath();
    ctx.arc(0, 0, F * 0.44, 0.4, 2.2);
    ctx.arc(0, 0, F * 0.44, 3.6, 5.4);
    stroke('rgba(120,235,255,.55)');
    ctx.restore();
    ctx.font = `${Math.round(F * 0.052)}px ui-monospace, monospace`;
    ctx.fillStyle = 'rgba(180,245,255,.9)';
    ctx.fillText(`FACE_ID ${hash(Math.floor(s.time)).toFixed(6).slice(2, 8)}`, x0, y0 - F * 0.02);
    ctx.fillText(`YAW ${(g.yaw * 57).toFixed(1)}°  PITCH ${(g.pitch * 40).toFixed(1)}°`, x0, y0 + bh + F * 0.07);
    ctx.fillText(`BEAUTY LOCK ✓`, x0, y0 + bh + F * 0.13);
    ctx.restore();
  }
  private bubble(ctx: CanvasRenderingContext2D, g: FaceGeom, F: number, s: OverlayState) {
    const [x, y] = this.px(...ptPair(get(g.pts, 57)));
    const grow = clamp(0.35 + s.openMouth * 1.6, 0, 1.4);
    const r = F * 0.2 * grow;
    if (r < 2) return;
    const grd = ctx.createRadialGradient(x - r * 0.3, y - r * 0.35, r * 0.1, x, y, r);
    grd.addColorStop(0, 'rgba(255,200,220,.95)');
    grd.addColorStop(0.55, 'rgba(248,110,150,.9)');
    grd.addColorStop(1, 'rgba(196,54,96,.85)');
    ctx.beginPath();
    ctx.arc(x, y, r, 0, Math.PI * 2);
    ctx.fillStyle = grd;
    ctx.fill();
    ctx.beginPath();
    ctx.ellipse(x - r * 0.32, y - r * 0.36, r * 0.22, r * 0.13, -0.5, 0, Math.PI * 2);
    ctx.fillStyle = 'rgba(255,255,255,.7)';
    ctx.fill();
  }

  /* ── frames ────────────────────────────────────────────────── */
  private drawFrame(ctx: CanvasRenderingContext2D, id: string, s: OverlayState) {
    const W = this.W;
    const H = this.H;
    const k = clamp(s.params.frameAlpha, 0, 1);
    ctx.save();
    ctx.globalAlpha = k;
    const m = Math.round(Math.min(W, H) * 0.045);
    if (id === 'polaroid') {
      ctx.fillStyle = '#f7f5ef';
      ctx.fillRect(0, 0, W, m);
      ctx.fillRect(0, H - m * 2.4, W, m * 2.4);
      ctx.fillRect(0, 0, m, H);
      ctx.fillRect(W - m, 0, m, H);
      ctx.fillStyle = 'rgba(0,0,0,.55)';
      ctx.font = `${Math.round(m * 0.42)}px ui-sans-serif, sans-serif`;
      ctx.fillText(dateStamp(), m * 1.2, H - m * 0.9);
    } else if (id === 'filmstrip') {
      ctx.fillStyle = '#0b0b0d';
      ctx.fillRect(0, 0, W, m * 1.3);
      ctx.fillRect(0, H - m * 1.3, W, m * 1.3);
      ctx.fillStyle = 'rgba(240,240,235,.9)';
      const n = Math.max(4, Math.floor(W / (m * 1.5)));
      for (let i = 0; i < n; i++) {
        const x = (i + 0.5) * (W / n);
        roundRect(ctx, x - m * 0.22, m * 0.35, m * 0.44, m * 0.4, m * 0.1);
        ctx.fill();
        roundRect(ctx, x - m * 0.22, H - m * 0.75, m * 0.44, m * 0.4, m * 0.1);
        ctx.fill();
      }
      ctx.font = `${Math.round(m * 0.4)}px ui-monospace, monospace`;
      ctx.fillText('HAO 400 · 24 EXP', m * 0.8, m * 0.95);
    } else if (id === 'haocam') {
      ctx.strokeStyle = 'rgba(255,255,255,.8)';
      ctx.lineWidth = Math.max(2, W * 0.0035);
      const c = m * 1.4;
      for (const [sx, sy] of [
        [1, 1],
        [-1, 1],
        [1, -1],
        [-1, -1],
      ] as const) {
        ctx.beginPath();
        const x = sx > 0 ? W - m * 0.5 : m * 0.5;
        const y = sy > 0 ? H - m * 0.5 : m * 0.5;
        ctx.moveTo(x - sx * c, y);
        ctx.lineTo(x, y);
        ctx.lineTo(x, y - sy * c);
        ctx.stroke();
      }
      ctx.fillStyle = 'rgba(255,255,255,.92)';
      ctx.font = `600 ${Math.round(m * 0.42)}px ui-sans-serif, sans-serif`;
      ctx.fillText('Shot on HaoCam', m * 0.7, H - m * 0.62);
      ctx.font = `${Math.round(m * 0.3)}px ui-monospace, monospace`;
      ctx.fillStyle = 'rgba(255,255,255,.6)';
      ctx.fillText(`WEBGL2 · ${dateStamp()}`, m * 0.7, H - m * 0.2);
    } else if (id === 'softvign') {
      const grd = ctx.createRadialGradient(W / 2, H / 2, Math.min(W, H) * 0.3, W / 2, H / 2, Math.max(W, H) * 0.72);
      grd.addColorStop(0, 'rgba(0,0,0,0)');
      grd.addColorStop(1, 'rgba(0,0,0,.55)');
      ctx.fillStyle = grd;
      ctx.fillRect(0, 0, W, H);
    } else if (id === 'ui916') {
      ctx.fillStyle = 'rgba(255,255,255,.9)';
      ctx.font = `700 ${Math.round(m * 0.44)}px ui-sans-serif, sans-serif`;
      ctx.fillText('◉ LIVE', m * 0.7, m * 1.0);
      ctx.textAlign = 'right';
      ctx.fillText('HaoCam · Beauty Cam', W - m * 0.7, m * 1.0);
      ctx.textAlign = 'left';
      const bh = m * 2.6;
      const grd = ctx.createLinearGradient(0, H - bh, 0, H);
      grd.addColorStop(0, 'rgba(0,0,0,0)');
      grd.addColorStop(1, 'rgba(0,0,0,.55)');
      ctx.fillStyle = grd;
      ctx.fillRect(0, H - bh, W, bh);
      ctx.fillStyle = 'rgba(255,255,255,.85)';
      ctx.font = `${Math.round(m * 0.36)}px ui-sans-serif, sans-serif`;
      ctx.fillText('♥ 12.4k     💬 892     ↪ share', m * 0.7, H - m * 0.7);
    }
    ctx.restore();
  }

  /** debug visual: landmark + kontur — berguna saat menyetel parameter */
  private meshDebug(ctx: CanvasRenderingContext2D, g: FaceGeom) {
    const W = this.W;
    const H = this.H;
    const r = Math.max(1.2, W / 700);
    ctx.save();
    ctx.lineWidth = Math.max(1, W / 1000);
    ctx.strokeStyle = 'rgba(120,255,190,.9)';
    ctx.fillStyle = 'rgba(255,90,120,.95)';
    const groups = [
      [0, 17, 21, 22, 26, 5, 4],
      [17, 18, 19, 20, 21],
      [22, 23, 24, 25, 26],
      [27, 28, 29, 30],
      [31, 32, 33, 34, 35],
      [36, 37, 38, 39, 40, 41, 36],
      [42, 43, 44, 45, 46, 47, 42],
      [48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 48],
      [60, 61, 62, 63, 64, 65, 66, 67, 60],
    ];
    for (const grp of groups) {
      ctx.beginPath();
      grp.forEach((idx, i) => {
        const x = g.pts[idx * 2] * W;
        const y = g.pts[idx * 2 + 1] * H;
        i ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
      });
      ctx.stroke();
    }
    for (let i = 0; i < 68; i++) {
      ctx.beginPath();
      ctx.arc(g.pts[i * 2] * W, g.pts[i * 2 + 1] * H, r, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();
  }
}

/* ── utilities ─────────────────────────────────────────────── */
function ptPair(p: { x: number; y: number }): [number, number] {
  return [p.x, p.y];
}
/** estimasi garis rambut (dipakai untuk ear/crown/halo placement) */
function hairline(g: FaceGeom) {
  const brow = mid(get(g.pts, 21), get(g.pts, 22));
  const fh = Math.max(get(g.pts, 8).y - brow.y, 1e-4);
  return { x: brow.x, y: brow.y - fh * 0.95 };
}
function hash(n: number) {
  const x = Math.sin(n * 12.9898) * 43758.5453;
  return x - Math.floor(x);
}
function dateStamp() {
  const d = new Date();
  const p = (v: number) => String(v).padStart(2, '0');
  return `${d.getFullYear()}.${p(d.getMonth() + 1)}.${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}`;
}
function roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
  const rr = Math.min(r, w / 2, h / 2);
  ctx.beginPath();
  ctx.moveTo(x + rr, y);
  ctx.arcTo(x + w, y, x + w, y + h, rr);
  ctx.arcTo(x + w, y + h, x, y + h, rr);
  ctx.arcTo(x, y + h, x, y, rr);
  ctx.arcTo(x, y, x + w, y, rr);
  ctx.closePath();
}
function star(ctx: CanvasRenderingContext2D, x: number, y: number, r: number, color: string) {
  if (r <= 0.4) return;
  ctx.save();
  ctx.translate(x, y);
  ctx.beginPath();
  for (let i = 0; i < 8; i++) {
    const a = (i / 8) * Math.PI * 2;
    const rad = i % 2 === 0 ? r : r * 0.34;
    const px = Math.cos(a) * rad;
    const py = Math.sin(a) * rad;
    i ? ctx.lineTo(px, py) : ctx.moveTo(px, py);
  }
  ctx.closePath();
  ctx.fillStyle = color;
  ctx.fill();
  ctx.restore();
}
function star4(ctx: CanvasRenderingContext2D, x: number, y: number, r: number, color: string) {
  if (r <= 0.4) return;
  ctx.save();
  ctx.translate(x, y);
  ctx.beginPath();
  for (let i = 0; i < 4; i++) {
    const a = (i / 4) * Math.PI * 2;
    const rad = r;
    const px = Math.cos(a) * rad;
    const py = Math.sin(a) * rad;
    const mx = Math.cos(a + Math.PI / 4) * rad * 0.28;
    const my = Math.sin(a + Math.PI / 4) * rad * 0.28;
    i ? ctx.lineTo(px, py) : ctx.moveTo(px, py);
    ctx.quadraticCurveTo(mx, my, Math.cos(a + Math.PI / 2) * rad, Math.sin(a + Math.PI / 2) * rad);
  }
  ctx.closePath();
  ctx.fillStyle = color;
  ctx.fill();
  ctx.restore();
}
function heart(ctx: CanvasRenderingContext2D, x: number, y: number, r: number, color: string, outline: boolean) {
  ctx.save();
  ctx.translate(x, y);
  ctx.beginPath();
  ctx.moveTo(0, r * 0.72);
  ctx.bezierCurveTo(-r * 1.35, -r * 0.15, -r * 0.55, -r * 1.05, 0, -r * 0.42);
  ctx.bezierCurveTo(r * 0.55, -r * 1.05, r * 1.35, -r * 0.15, 0, r * 0.72);
  ctx.closePath();
  if (outline) {
    ctx.fillStyle = color;
    ctx.shadowColor = 'rgba(255,80,120,.6)';
    ctx.shadowBlur = r * 0.5;
    ctx.fill();
    ctx.shadowBlur = 0;
    ctx.lineWidth = r * 0.1;
    ctx.strokeStyle = 'rgba(255,255,255,.55)';
    ctx.stroke();
  } else {
    ctx.fillStyle = color;
    ctx.fill();
  }
  ctx.restore();
}

