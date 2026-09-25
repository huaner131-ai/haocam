/**
 * Pipeline — render graph HaoCam.
 *
 *   1. input      : video/canvas → texSrc (cover-fit, zoom, rotate, mirror)
 *   2. warp       : mesh Delaunay (dest↔src) → texWarp  (V-line, mata, hidung, bibir…)
 *   3. pyramid    : ½ ¼ ⅛ bilateral → dipakai smoothing / clarity / makeup
 *   4. masks      : raster region (3×RGBA) di res rendah lalu blur H/V
 *   5. skin       : smoothing edge-aware + tone even + blemish + whiten + matte + glow
 *   6. features   : sclera, iris, catchlight, mata panda, gigi
 *   7. makeup     : liner/lash, alis, shadow, blush, bibir, highlight, contour, freckle
 *   8. bloom      : bright-pass + blur piramida
 *   9. grade      : ring light, soft focus, clarity, LUT, film, grain, vignette
 *  10. present    : + overlay lens/frame (Canvas2D) → canvas (yang direkam)
 */
import { Gfx, Program, Target } from '../gl/gfx';
import {
  FS_BRIGHT,
  FS_BLUR2,
  FS_DOWNBLUR,
  FS_FEATURES,
  FS_GRADE,
  FS_INPUT,
  FS_MAKEUP,
  FS_MASK,
  FS_PRESENT,
  FS_SKIN,
  FS_WARP_SAMPLE,
  VS_MASK,
  VS_MESH,
} from '../gl/shaders';
import type { FaceGeom } from '../face/landmarks';
import { FaceMesh, buildGeom } from '../face/mesh';
import { MaskBuilder } from './masks';
import type { FxParams } from './params';
import type { Lut } from './lut';

export interface RenderInput {
  source: HTMLVideoElement | HTMLCanvasElement | ImageBitmap;
  srcW: number;
  srcH: number;
  geom: FaceGeom | null;
  params: FxParams;
  overlay: HTMLCanvasElement | null;
  time: number;
  compare: number; // 0..1 posisi garis compare, 0 = off
  flash: number; // 0..1
  warpStrength: number; // 0..1 global (difade saat no-face)
  autoExp: number; // -0.5..0.5 (AE loop dari engine)
}

export interface PipelineStats {
  outW: number;
  outH: number;
  drawMs: number;
  tris: number;
  maskVerts: number;
  lutName: string | null;
}

const VSQ = /* glsl */ `
layout(location = 0) in vec2 aPos;
out vec2 vUv;
void main() {
  vUv = aPos * 0.5 + 0.5;
  vUv.y = 1.0 - vUv.y;
  gl_Position = vec4(aPos, 0.0, 1.0);
}`;

const EMPTY_GEOM: FaceGeom = {
  pts: new Float32Array(136),
  vis: 0,
  box: [0, 0, 0, 0],
  center: { x: 0.5, y: 0.5 },
  width: 0,
  roll: 0,
  yaw: 0,
  pitch: 0,
  eyeR: { c: { x: 0, y: 0 }, rx: 0, ry: 0, angle: 0 },
  eyeL: { c: { x: 0, y: 0 }, rx: 0, ry: 0, angle: 0 },
  mouth: { c: { x: 0, y: 0 }, rx: 0, ry: 0 },
  happy: 0,
  surprised: 0,
  blink: 0,
};

interface Targets {
  src: Target;
  warp: Target;
  a: Target;
  b: Target;
  p0: Target;
  p1: Target;
  p2: Target;
  m: [Target, Target, Target];
  mTmp: [Target, Target, Target];
  bloom: Target;
  bloom2: Target;
}

export class Pipeline {
  readonly gfx: Gfx;
  private mesh = new FaceMesh();
  private masks = new MaskBuilder();
  private progs: Record<string, Program> = {};
  private t: Targets | null = null;
  private meshVbo!: WebGLBuffer;
  private meshIbo!: WebGLBuffer;
  private meshVao!: WebGLVertexArrayObject;
  private maskVao: WebGLVertexArrayObject[] = [];
  private maskVbo: WebGLBuffer[] = [];
  private videoTex: WebGLTexture | null = null;
  private videoTexW = 0;
  private videoTexH = 0;
  private overlayTex: WebGLTexture | null = null;
  private overlayW = 0;
  private overlayH = 0;
  private lutTex: WebGLTexture | null = null;
  private lutSize = 32;
  lutName: string | null = null;
  private outW = 0;
  private outH = 0;
  private indexCount = 0;
  private triCooldown = 0;
  private warned = false;
  private fitM = new Float32Array(9);
  private eyeBuf = new Float32Array(4);
  private viewPts = new Float32Array(68 * 2);
  stats: PipelineStats = { outW: 0, outH: 0, drawMs: 0, tris: 0, maskVerts: 0, lutName: null };
  glError = 0;

  constructor(canvas: HTMLCanvasElement) {
    this.gfx = new Gfx(canvas);
    const gl = this.gfx.gl;
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.CULL_FACE);
    gl.disable(gl.SCISSOR_TEST);

    this.meshVbo = gl.createBuffer()!;
    this.meshIbo = gl.createBuffer()!;
    this.meshVao = gl.createVertexArray()!;
    gl.bindVertexArray(this.meshVao);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.meshVbo);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 16, 0);
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 16, 8);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.meshIbo);

    for (let i = 0; i < 3; i++) {
      const vao = gl.createVertexArray()!;
      const vbo = gl.createBuffer()!;
      gl.bindVertexArray(vao);
      gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
      gl.enableVertexAttribArray(0);
      gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 24, 0);
      gl.enableVertexAttribArray(1);
      gl.vertexAttribPointer(1, 4, gl.FLOAT, false, 24, 8);
      this.maskVao.push(vao);
      this.maskVbo.push(vbo);
    }
    gl.bindVertexArray(null);
    this.buildPrograms();
  }

  private buildPrograms() {
    const g = this.gfx;
    this.progs.input = g.link('input', VSQ, FS_INPUT);
    this.progs.down = g.link('down', VSQ, FS_DOWNBLUR);
    this.progs.warp = g.link('warp', VS_MESH, FS_WARP_SAMPLE, { common: false });
    this.progs.mask = g.link('mask', VS_MASK, FS_MASK, { common: false });
    this.progs.blur = g.link('blur', VSQ, FS_BLUR2);
    this.progs.skin = g.link('skin', VSQ, FS_SKIN);
    this.progs.features = g.link('features', VSQ, FS_FEATURES);
    this.progs.makeup = g.link('makeup', VSQ, FS_MAKEUP);
    this.progs.bright = g.link('bright', VSQ, FS_BRIGHT);
    this.progs.grade = g.link('grade', VSQ, FS_GRADE);
    this.progs.present = g.link('present', VSQ, FS_PRESENT);
  }

  get canvas() {
    return this.gfx.canvas;
  }
  get width() {
    return this.outW;
  }
  get height() {
    return this.outH;
  }

  resize(cssW: number, cssH: number, aspect: number, qualityScale: number, maskScale: number): boolean {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const cap = 1440;
    let outW = Math.max(240, Math.round(cssW * dpr * qualityScale));
    let outH = Math.max(240, Math.round(cssH * dpr * qualityScale));
    if (aspect > 0) {
      if (aspect < 1) {
        outH = Math.min(outH, cap);
        outW = Math.round(outH * aspect);
      } else {
        outW = Math.min(outW, cap);
        outH = Math.round(outW / aspect);
      }
    } else {
      const long = Math.max(outW, outH);
      if (long > cap) {
        const k = cap / long;
        outW = Math.round(outW * k);
        outH = Math.round(outH * k);
      }
    }
    outW -= outW % 2;
    outH -= outH % 2;
    const max = this.gfx.maxTex;
    outW = Math.min(outW, max);
    outH = Math.min(outH, max);
    if (outW === this.outW && outH === this.outH) return false;

    const c = this.gfx.canvas;
    c.width = outW;
    c.height = outH;
    this.outW = outW;
    this.outH = outH;

    const d2 = (v: number, s: number) => Math.max(8, Math.round(v * s));
    const mw = d2(outW, maskScale);
    const mh = d2(outH, maskScale);
    const old = this.t;
    this.t = {
      src: this.gfx.target(outW, outH),
      warp: this.gfx.target(outW, outH),
      a: this.gfx.target(outW, outH),
      b: this.gfx.target(outW, outH),
      p0: this.gfx.target(d2(outW, 0.5), d2(outH, 0.5)),
      p1: this.gfx.target(d2(outW, 0.25), d2(outH, 0.25)),
      p2: this.gfx.target(d2(outW, 0.125), d2(outH, 0.125)),
      m: [this.gfx.target(mw, mh), this.gfx.target(mw, mh), this.gfx.target(mw, mh)],
      mTmp: [this.gfx.target(mw, mh), this.gfx.target(mw, mh), this.gfx.target(mw, mh)],
      bloom: this.gfx.target(d2(outW, 0.25), d2(outH, 0.25)),
      bloom2: this.gfx.target(d2(outW, 0.25), d2(outH, 0.25)),
    };
    if (old) {
      old.src.dispose();
      old.warp.dispose();
      old.a.dispose();
      old.b.dispose();
      old.p0.dispose();
      old.p1.dispose();
      old.p2.dispose();
      for (const x of old.m) x.dispose();
      for (const x of old.mTmp) x.dispose();
      old.bloom.dispose();
      old.bloom2.dispose();
    }
    return true;
  }

  /** uv output → uv kamera (cover-fit + zoom + rotate + mirror), diekstrak dari pemetaan 3 titik */
  /**
   * Proyeksi landmark (ruang kamera, sudah di-mirror tracker) → ruang output.
   * Wajib: output = cover-crop + zoom + rotate + mirror, jadi koordinat video
   * tidak bisa dipakai mentah untuk mask / warp / lens.
   */
  toViewSpace(geom: FaceGeom | null, srcW: number, srcH: number, p: FxParams): FaceGeom | null {
    if (!geom || !this.outW) return geom;
    const m = this.computeFit(srcW, srcH, p);
    const a = m[0], b = m[3], tx = m[6];
    const c = m[1], d = m[4], ty = m[7];
    const det = a * d - b * c;
    if (!Number.isFinite(det) || Math.abs(det) < 1e-9) return geom;
    const ia = d / det, ib = -b / det, ic = -c / det, id = a / det;
    const out = this.viewPts;
    for (let i = 0; i < 68; i++) {
      const x = geom.pts[i * 2] - tx;
      const y = geom.pts[i * 2 + 1] - ty;
      out[i * 2] = ia * x + ib * y;
      out[i * 2 + 1] = ic * x + id * y;
    }
    let minX = 9, maxX = -9, minY = 9, maxY = -9;
    const corners: [number, number][] = [
      [geom.box[0], geom.box[1]],
      [geom.box[0] + geom.box[2], geom.box[1] + geom.box[3]],
    ];
    for (const [ux, uy] of corners) {
      const x = ux - tx;
      const y = uy - ty;
      const u = ia * x + ib * y;
      const v = ic * x + id * y;
      minX = Math.min(minX, u);
      maxX = Math.max(maxX, u);
      minY = Math.min(minY, v);
      maxY = Math.max(maxY, v);
    }
    return buildGeom(out, geom.vis, [minX, minY, maxX - minX, maxY - minY], {
      happy: geom.happy,
      surprised: geom.surprised,
      neutral: Math.max(0, 1 - geom.happy - geom.surprised),
    });
  }

  private computeFit(srcW: number, srcH: number, p: FxParams) {
    const R = ((Math.round(p.rotate / 90) % 4) + 4) % 4;
    const odd = R % 2 === 1;
    const rsW = odd ? srcH : srcW;
    const rsH = odd ? srcW : srcH;
    const zoom = Math.max(p.zoom, 0.05);
    const scale = Math.max(this.outW / Math.max(rsW, 1), this.outH / Math.max(rsH, 1)) * zoom;
    const Sx = Math.max(rsW * scale, 1);
    const Sy = Math.max(rsH * scale, 1);
    const W = this.outW;
    const H = this.outH;
    const mirror = p.mirror > 0.5;
    const f = (x: number, y: number): [number, number] => {
      const dx = (x - 0.5) * W;
      const dy = (y - 0.5) * H;
      let qx: number, qy: number;
      if (R === 0) [qx, qy] = [dx, dy];
      else if (R === 1) [qx, qy] = [dy, -dx];
      else if (R === 2) [qx, qy] = [-dx, -dy];
      else [qx, qy] = [-dy, dx];
      let u = qx / Sx + 0.5;
      let v = qy / Sy + 0.5;
      if (R === 1) [u, v] = [v, 1 - u];
      else if (R === 2) [u, v] = [1 - u, 1 - v];
      else if (R === 3) [u, v] = [1 - v, u];
      if (mirror) u = 1 - u;
      return [u, v];
    };
    const a = f(0, 0);
    const b = f(1, 0);
    const c = f(0, 1);
    const m = this.fitM;
    m[0] = b[0] - a[0];
    m[1] = b[1] - a[1];
    m[2] = 0;
    m[3] = c[0] - a[0];
    m[4] = c[1] - a[1];
    m[5] = 0;
    m[6] = a[0];
    m[7] = a[1];
    m[8] = 1;
    return m;
  }

  setLut(lut: Lut | null) {
    const gl = this.gfx.gl;
    if (!lut) {
      if (this.lutTex) gl.deleteTexture(this.lutTex);
      this.lutTex = null;
      this.lutName = null;
      return;
    }
    if (!this.lutTex) this.lutTex = gl.createTexture()!;
    this.lutSize = lut.size;
    gl.bindTexture(gl.TEXTURE_3D, this.lutTex);
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.texImage3D(gl.TEXTURE_3D, 0, gl.RGBA8, lut.size, lut.size, lut.size, 0, gl.RGBA, gl.UNSIGNED_BYTE, lut.data);
    gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_R, gl.CLAMP_TO_EDGE);
    gl.bindTexture(gl.TEXTURE_3D, null);
    this.lutName = lut.name;
  }

  private uploadVideo(src: RenderInput['source'], w: number, h: number) {
    const gl = this.gfx.gl;
    if (!this.videoTex || this.videoTexW !== w || this.videoTexH !== h) {
      if (this.videoTex) gl.deleteTexture(this.videoTex);
      this.videoTex = gl.createTexture()!;
      this.videoTexW = w;
      this.videoTexH = h;
    }
    gl.bindTexture(gl.TEXTURE_2D, this.videoTex);
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, src as TexImageSource);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return this.videoTex;
  }

  private uploadOverlay(cv: HTMLCanvasElement) {
    const gl = this.gfx.gl;
    if (!this.overlayTex || this.overlayW !== cv.width || this.overlayH !== cv.height) {
      if (this.overlayTex) gl.deleteTexture(this.overlayTex);
      this.overlayTex = gl.createTexture()!;
      this.overlayW = cv.width;
      this.overlayH = cv.height;
    }
    gl.bindTexture(gl.TEXTURE_2D, this.overlayTex);
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
    gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, cv);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return this.overlayTex;
  }

  render(req: RenderInput) {
    const t = this.t;
    if (!t) return;
    const g = this.gfx;
    const gl = g.gl;
    const P = this.progs;
    const p = req.params;
    const geom = req.geom;
    const hasFace = !!geom && geom.vis > 0.02;
    const t0 = performance.now();

    /* 1 ─ input ------------------------------------------------ */
    const vtex = this.uploadVideo(req.source, req.srcW, req.srcH);
    g.bindTarget(t.src);
    const inp = P.input;
    gl.useProgram(inp.id);
    inp.setTex('uTex', vtex, 0);
    inp.setMat3('uFit', this.computeFit(req.srcW, req.srcH, p));
    inp.set('uSharpen', 0);
    g.drawQuad();

    /* 2 ─ mesh warp -------------------------------------------- */
    let warpActive = false;
    if (geom && hasFace) {
      this.mesh.layout(geom);
      if (this.triCooldown <= 0) {
        this.mesh.triangulate();
        this.triCooldown = 3;
      }
      this.triCooldown--;
      const warpSum =
        Math.abs(p.jawSlim) +
        p.cheek +
        Math.abs(p.chin) +
        Math.abs(p.forehead) +
        p.eyeEnlarge +
        Math.abs(p.eyeSpacing) +
        p.noseSlim +
        Math.abs(p.noseShort) +
        p.lipPlump +
        p.smile +
        p.browLift +
        p.earFade;
      if (warpSum > 0.004 && this.mesh.indexCount > 0 && req.warpStrength > 0.02) {
        warpActive = true;
        this.mesh.displace(geom, p, this.outW / this.outH, req.warpStrength);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.meshVbo);
        gl.bufferData(gl.ARRAY_BUFFER, this.mesh.verts.byteLength, gl.DYNAMIC_DRAW);
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, this.mesh.verts);
        if (this.indexCount !== this.mesh.indexCount) {
          gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.meshIbo);
          gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, this.mesh.indices.subarray(0, this.mesh.indexCount), gl.DYNAMIC_DRAW);
          this.indexCount = this.mesh.indexCount;
        }
        const wp = P.warp;
        g.bindTarget(t.warp);
        gl.useProgram(wp.id);
        wp.setTex('uTex', t.src.tex, 0);
        gl.bindVertexArray(this.meshVao);
        gl.drawElements(gl.TRIANGLES, this.indexCount, gl.UNSIGNED_INT, 0);
        gl.bindVertexArray(null);
      }
    }
    const cur = warpActive ? t.warp : t.src;

    /* 3 ─ pyramid ---------------------------------------------- */
    this.down(t.src.tex, t.p0, [this.outW, this.outH], 1.0, 0.1, 0.75);
    this.down(t.p0.tex, t.p1, [t.p0.w, t.p0.h], 1.25, 0.09, 0.85);
    this.down(t.p1.tex, t.p2, [t.p1.w, t.p1.h], 1.5, 0.08, 0.95);

    /* 4 ─ masks ------------------------------------------------ */
    const mf = this.masks.build(this.mesh.dest, geom ?? EMPTY_GEOM, p, warpActive);
    gl.useProgram(P.mask.id);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE);
    const counts = [mf.an, mf.bn, mf.cn];
    const arrs = [mf.a, mf.b, mf.c];
    for (let i = 0; i < 3; i++) {
      const n = counts[i];
      g.bindTarget(t.m[i], true);
      if (n > 2) {
        gl.bindVertexArray(this.maskVao[i]);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.maskVbo[i]);
        gl.bufferData(gl.ARRAY_BUFFER, n * 24, gl.DYNAMIC_DRAW);
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, arrs[i].subarray(0, n * 6));
        gl.drawArrays(gl.TRIANGLES, 0, n);
        gl.bindVertexArray(null);
      }
    }
    gl.disable(gl.BLEND);
    for (let pass = 0; pass < 2; pass++) {
      for (let i = 0; i < 3; i++) this.blur(t.m[i], t.mTmp[i], [1, 0], pass === 0 ? 1.4 : 2.6, 0);
      for (let i = 0; i < 3; i++) this.blur(t.mTmp[i], t.m[i], [0, 1], pass === 0 ? 1.4 : 2.6, 0);
    }

    /* 5 ─ skin ------------------------------------------------- */
    const sk = P.skin;
    g.bindTarget(t.a);
    gl.useProgram(sk.id);
    sk.setTex('uSrc', cur.tex, 0)
      .setTex('uPyr0', t.p0.tex, 1)
      .setTex('uPyr1', t.p1.tex, 2)
      .setTex('uPyr2', t.p2.tex, 3)
      .setTex('uMaskA', t.m[0].tex, 4)
      .setTex('uMaskB', t.m[1].tex, 5)
      .setTex('uMaskC', t.m[2].tex, 6);
    sk.set('uSrcTexel', [1 / this.outW, 1 / this.outH]);
    sk.set('uSmooth', hasFace ? p.smooth : p.smooth * 0.2);
    sk.set('uTexture', p.texture);
    sk.set('uWhiten', hasFace ? p.whitening : 0);
    sk.set('uToneEven', hasFace ? p.toneEven : p.toneEven * 0.35);
    sk.set('uBlemish', hasFace ? p.blemish : 0);
    sk.set('uMatte', hasFace ? p.matte : 0);
    sk.set('uGlow', hasFace ? p.glow : p.glow * 0.4);
    sk.set('uGlowAmt', 1);
    sk.set('uSmoothMode', p.smoothMode);
    g.drawQuad();

    /* 6 ─ features --------------------------------------------- */
    const ft = P.features;
    g.bindTarget(t.b);
    gl.useProgram(ft.id);
    ft.setTex('uSrc', t.a.tex, 0).setTex('uPyr1', t.p1.tex, 1);
    ft.setTex('uMaskA', t.m[0].tex, 4).setTex('uMaskB', t.m[1].tex, 5).setTex('uMaskC', t.m[2].tex, 6);
    ft.set('uSrcTexel', [1 / this.outW, 1 / this.outH]);
    const wg = (warpActive && mf.wgeom) || geom;
    if (wg && hasFace) {
      this.eyeBuf[0] = wg.eyeR.c.x;
      this.eyeBuf[1] = wg.eyeR.c.y;
      this.eyeBuf[2] = wg.eyeL.c.x;
      this.eyeBuf[3] = wg.eyeL.c.y;
      ft.setVec2Array('uEyeC', this.eyeBuf, 2);
      this.eyeBuf[0] = wg.eyeR.rx * 1.15;
      this.eyeBuf[1] = wg.eyeR.ry * 1.15;
      this.eyeBuf[2] = wg.eyeL.rx * 1.15;
      this.eyeBuf[3] = wg.eyeL.ry * 1.15;
      ft.setVec2Array('uEyeR', this.eyeBuf, 2);
    } else {
      this.eyeBuf.fill(0);
      ft.setVec2Array('uEyeC', this.eyeBuf, 2);
      ft.setVec2Array('uEyeR', this.eyeBuf, 2);
    }
    const f = (v: number) => (hasFace ? v : 0);
    ft.set('uEyeWhite', f(p.eyeWhite))
      .set('uEyeBright', f(p.eyeBright))
      .set('uUndereye', f(p.undereye))
      .set('uIrisPop', f(p.irisPop))
      .set('uCatch', f(p.catchlight))
      .set('uTeeth', f(p.teeth))
      .set('uTeethPolish', f(p.teethPolish))
      .set('uTime', req.time);
    g.drawQuad();

    /* 7 ─ makeup ----------------------------------------------- */
    const mk = P.makeup;
    g.bindTarget(t.a);
    gl.useProgram(mk.id);
    mk.setTex('uSrc', t.b.tex, 0).setTex('uPyr0', t.p0.tex, 1);
    mk.setTex('uMaskA', t.m[0].tex, 4).setTex('uMaskB', t.m[1].tex, 5).setTex('uMaskC', t.m[2].tex, 6);
    mk.set('uSrcTexel', [1 / this.outW, 1 / this.outH]);
    mk.set('uOn', p.makeupOn)
      .set('uLashes', f(p.lashes))
      .set('uLiner', f(p.liner))
      .set('uBrowFill', f(p.browFill))
      .set('uShadow', f(p.shadow))
      .set('uBlush', f(p.blush))
      .set('uLipStrength', f(p.lipStrength))
      .set('uLipGloss', f(p.lipGloss))
      .set('uHighlight', f(p.highlight))
      .set('uContour', f(p.contour))
      .set('uFreckle', f(p.freckle))
      .set('uGlass', p.tint);
    mk.set('uLinerCol', [0.05, 0.04, 0.06]);
    mk.set('uBrowCol', [p.browColor.r, p.browColor.g, p.browColor.b]);
    mk.set('uShadowCol', [p.shadowColor.r, p.shadowColor.g, p.shadowColor.b]);
    mk.set('uBlushCol', [p.blushColor.r, p.blushColor.g, p.blushColor.b]);
    mk.set('uLipCol', [p.lipColor.r, p.lipColor.g, p.lipColor.b]);
    mk.set('uHiCol', [1.0, 0.94, 0.88]);
    mk.set('uContourCol', [0.55, 0.4, 0.33]);
    g.drawQuad();

    /* 8 ─ bloom ------------------------------------------------ */
    const needBloom = p.bloom > 0.001 || p.halation > 0.001 || p.softFocus > 0.001 || p.glow > 0.001;
    if (needBloom) {
      const br = P.bright;
      g.bindTarget(t.bloom);
      gl.useProgram(br.id);
      br.setTex('uTex', t.a.tex, 0);
      br.set('uThresh', 0.5);
      br.set('uSoft', 0.24);
      g.drawQuad();
      for (const r of [1.5, 3, 6]) {
        this.blur(t.bloom, t.bloom2, [1, 0], r, 1);
        this.blur(t.bloom2, t.bloom, [0, 1], r, 1);
      }
    } else {
      g.bindTarget(t.bloom, true);
    }

    /* 9 ─ grade ------------------------------------------------ */
    const gr = P.grade;
    g.bindTarget(t.b);
    gl.useProgram(gr.id);
    gr.setTex('uSrc', t.a.tex, 0).setTex('uPyr1', t.p1.tex, 1).setTex('uBloom', t.bloom.tex, 2);
    gr.setTex('uMaskA', t.m[0].tex, 4).setTex('uMaskB', t.m[1].tex, 5).setTex('uMaskC', t.m[2].tex, 6);
    if (this.lutTex) gr.setTex('uLut', this.lutTex, 7);
    gr.set('uRes', [this.outW, this.outH]);
    gr.set('uLutSize', this.lutSize);
    gr.set('uHasLut', this.lutTex ? 1 : 0);
    gr.set('uLutMix', this.lutTex ? p.lutStrength : 0);
    gr.set('uExposure', p.exposure)
      .set('uContrast', p.contrast)
      .set('uSat', p.saturation)
      .set('uTemp', p.temp)
      .set('uTint', p.tint2)
      .set('uHi', p.highlights)
      .set('uSh', p.shadows)
      .set('uFade', p.fade)
      .set('uVig', p.vignette)
      .set('uGrain', p.grain)
      .set('uBloomAmt', p.bloom)
      .set('uHalation', p.halation)
      .set('uSoft', p.softFocus)
      .set('uSharp', p.sharpness)
      .set('uClarity', p.clarity)
      .set('uRing', p.ringLight)
      .set('uTime', req.time)
      .set('uAutoExp', req.autoExp);
    g.drawQuad();

    /* 10 ─ present --------------------------------------------- */
    const pr = P.present;
    gl.useProgram(pr.id);
    pr.setTex('uSrc', t.b.tex, 0).setTex('uRaw', t.src.tex, 1);
    if (req.overlay && req.overlay.width > 1) {
      pr.setTex('uOverlay', this.uploadOverlay(req.overlay), 2);
    }
    pr.set('uSplit', req.compare > 0 ? req.compare : 0);
    pr.set('uFlash', req.flash);
    g.bindTarget(null);
    g.drawQuad();

    this.glError = gl.getError();
    if (this.glError !== gl.NO_ERROR && !this.warned) {
      this.warned = true;
      console.warn('[pipeline] GL error 0x' + this.glError.toString(16));
    }
    this.stats.drawMs = performance.now() - t0;
    this.stats.outW = this.outW;
    this.stats.outH = this.outH;
    this.stats.tris = this.indexCount / 3;
    this.stats.maskVerts = counts[0] + counts[1] + counts[2];
    this.stats.lutName = this.lutName;
  }

  private down(srcTex: WebGLTexture, dst: Target, srcSize: [number, number], sigma: number, range: number, weight: number) {
    const gl = this.gfx.gl;
    const d = this.progs.down;
    this.gfx.bindTarget(dst);
    gl.useProgram(d.id);
    d.setTex('uTex', srcTex, 0);
    d.set('uSrcTexel', [1 / srcSize[0], 1 / srcSize[1]]);
    d.set('uSigma', sigma).set('uRange', range).set('uWeight', weight);
    this.gfx.drawQuad();
  }

  private blur(src: Target, dst: Target, dir: [number, number], radius: number, keepAlpha: number) {
    const gl = this.gfx.gl;
    const b = this.progs.blur;
    this.gfx.bindTarget(dst);
    gl.useProgram(b.id);
    b.setTex('uTex', src.tex, 0);
    b.set('uTexel', [1 / src.w, 1 / src.h]);
    b.set('uDir', dir);
    b.set('uRadius', radius);
    b.set('uKeepAlpha', keepAlpha);
    this.gfx.drawQuad();
  }

  dispose() {
    const gl = this.gfx.gl;
    if (this.t) {
      const t = this.t;
      t.src.dispose();
      t.warp.dispose();
      t.a.dispose();
      t.b.dispose();
      t.p0.dispose();
      t.p1.dispose();
      t.p2.dispose();
      for (const x of t.m) x.dispose();
      for (const x of t.mTmp) x.dispose();
      t.bloom.dispose();
      t.bloom2.dispose();
      this.t = null;
    }
    if (this.videoTex) gl.deleteTexture(this.videoTex);
    if (this.overlayTex) gl.deleteTexture(this.overlayTex);
    if (this.lutTex) gl.deleteTexture(this.lutTex);
    gl.deleteBuffer(this.meshVbo);
    gl.deleteBuffer(this.meshIbo);
    gl.deleteVertexArray(this.meshVao);
    for (const b of this.maskVbo) gl.deleteBuffer(b);
    for (const v of this.maskVao) gl.deleteVertexArray(v);
  }
}
