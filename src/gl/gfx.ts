/**
 * Gfx — lapisan WebGL2 paling tipis yang dibutuhkan pipeline HaoCam.
 *
 * Desain (sama seperti engine kamera beauty native: satu "render graph" kecil,
 * ping-pong FBO RGBA8, shader fullscreen + satu shader mesh untuk warp/mask):
 *
 *   video → [srcTex] → mesh-warp → skin → features → makeup → bloom → grade → present
 *
 * Konvensi koordinat: SEMUA ruang gambar memakai y-down (v=0 = atas), sehingga
 * landmark, mask, dan displacement handle memakai sistem yang sama.
 */

export type UniformValue =
  | number
  | number[]
  | Float32Array
  | Int32Array
  | WebGLTexture
  | [number, number, number]
  | [number, number];

export interface TargetOpts {
  /** GL_LINEAR (default) atau GL_NEAREST */
  filter?: number;
  /** default GL.RGBA8 */
  internal?: number;
}

export class Program {
  readonly id: WebGLProgram;
  private readonly loc = new Map<string, WebGLUniformLocation | null>();
  private readonly arrayCache = new Map<string, Float32Array>();
  used = false;

  constructor(
    private gl: WebGL2RenderingContext,
    readonly name: string,
    id: WebGLProgram
  ) {
    this.id = id;
    const count = gl.getProgramParameter(id, gl.ACTIVE_UNIFORMS) as number;
    for (let i = 0; i < count; i++) {
      const info = gl.getActiveUniform(id, i);
      if (!info) continue;
      // nama array "uFoo[0]" → "uFoo"
      const clean = info.name.replace(/\[0\]$/, '');
      this.loc.set(clean, gl.getUniformLocation(id, info.name));
    }
  }

  has(name: string) {
    return this.loc.has(name);
  }

  set(name: string, value: UniformValue | WebGLTexture | null) {
    const gl = this.gl;
    const loc = this.loc.get(name);
    if (loc === undefined) return this; // di-optimasi keluar oleh driver / salah nama
    if (value === null) {
      gl.uniform1i(loc, 0);
      return this;
    }
    if (typeof value === 'number') {
      // Program menyimpan tipe uniform dari panggilan pertama; pakai float utk
      // semua scalar — untuk int, gunakan set1i() eksplisit.
      gl.uniform1f(loc, value);
    } else if (value instanceof WebGLTexture) {
      throw new Error(`${this.name}.${name}: texture harus lewat setTex()`);
    } else if (value instanceof Float32Array) {
      gl.uniform2fv(loc, value);
    } else if (Array.isArray(value)) {
      const n = value.length;
      if (n === 2) gl.uniform2fv(loc, value as number[]);
      else if (n === 3) gl.uniform3fv(loc, value as number[]);
      else if (n === 4) gl.uniform4fv(loc, value as number[]);
      else gl.uniform1fv(loc, value as number[]);
    }
    return this;
  }

  /** array vec2 (data landmark / handle warp) — pakai cache supaya zero-alloc */
  setVec2Array(name: string, data: Float32Array, count: number) {
    const loc = this.loc.get(name);
    if (loc === undefined) return this;
    let arr = this.arrayCache.get(name);
    if (!arr || arr.length < count * 2) {
      arr = new Float32Array(count * 2);
      this.arrayCache.set(name, arr);
    }
    arr.set(data.subarray(0, count * 2));
    this.gl.uniform2fv(loc, arr.subarray(0, count * 2));
    return this;
  }

  setVec4Array(name: string, data: Float32Array) {
    const loc = this.loc.get(name);
    if (loc !== undefined) this.gl.uniform4fv(loc, data);
    return this;
  }

  setMat3(name: string, m: Float32Array) {
    const loc = this.loc.get(name);
    if (loc !== undefined) this.gl.uniformMatrix3fv(loc, false, m);
    return this;
  }

  set1i(name: string, v: number) {
    const loc = this.loc.get(name);
    if (loc !== undefined) this.gl.uniform1i(loc, v);
    return this;
  }

  setTex(name: string, tex: WebGLTexture | null, unit: number) {
    const gl = this.gl;
    const loc = this.loc.get(name);
    if (loc === undefined) return this;
    gl.activeTexture(gl.TEXTURE0 + unit);
    if (tex) gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.uniform1i(loc, unit);
    return this;
  }
}

export class Target {
  tex: WebGLTexture;
  fb: WebGLFramebuffer;
  constructor(
    public w: number,
    public h: number,
    readonly filter: number,
    readonly internal: number
  ) {
    const gl = Gfx.ctx;
    this.tex = gl.createTexture()!;
    gl.bindTexture(gl.TEXTURE_2D, this.tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, internal, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    this.fb = gl.createFramebuffer()!;
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fb);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.tex, 0);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  dispose() {
    const gl = Gfx.ctx;
    gl.deleteTexture(this.tex);
    gl.deleteFramebuffer(this.fb);
  }
}

export class Gfx {
  static ctx: WebGL2RenderingContext;
  readonly gl: WebGL2RenderingContext;
  readonly quadVao: WebGLVertexArrayObject;
  private programs = new Map<string, Program>();

  constructor(readonly canvas: HTMLCanvasElement) {
    const gl = canvas.getContext('webgl2', {
      alpha: false,
      antialias: false,
      depth: false,
      stencil: false,
      premultipliedAlpha: false,
      preserveDrawingBuffer: false,
      powerPreference: 'high-performance',
      desynchronized: true,
    });
    if (!gl) throw new Error('WebGL2 tidak tersedia di browser ini.');
    this.gl = gl;
    Gfx.ctx = gl;

    // fullscreen triangle (1 draw, tanpa index buffer)
    this.quadVao = gl.createVertexArray()!;
    gl.bindVertexArray(this.quadVao);
    const buf = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buf);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
    gl.bindVertexArray(null);
  }

  get maxTex() {
    return this.gl.getParameter(this.gl.MAX_TEXTURE_SIZE) as number;
  }

  private compile(type: number, src: string): WebGLShader {
    const gl = this.gl;
    const sh = gl.createShader(type)!;
    gl.shaderSource(sh, src);
    gl.compileShader(sh);
    if (!gl.getShaderParameter(sh, gl.COMPILE_STATUS)) {
      const log = gl.getShaderInfoLog(sh) ?? 'unknown';
      const kind = type === gl.VERTEX_SHADER ? 'vertex' : 'fragment';
      throw new Error(`${kind} shader gagal:\n${log}\n${withLineNumbers(src)}`);
    }
    return sh;
  }

  link(name: string, vsBody: string, fsBody: string, opts?: { common?: boolean }): Program {
    const gl = this.gl;
    const common = opts?.common === false ? '' : GLSL_COMMON;
    const vs = this.compile(gl.VERTEX_SHADER, header(gl, vsBody));
    const fs = this.compile(gl.FRAGMENT_SHADER, header(gl, common + fsBody));
    const p = gl.createProgram()!;
    gl.attachShader(p, vs);
    gl.attachShader(p, fs);
    gl.bindAttribLocation(p, 0, 'aPos');
    gl.linkProgram(p);
    gl.deleteShader(vs);
    gl.deleteShader(fs);
    if (!gl.getProgramParameter(p, gl.LINK_STATUS)) {
      throw new Error(`link ${name} gagal: ${gl.getProgramInfoLog(p)}`);
    }
    const prog = new Program(gl, name, p);
    this.programs.set(name, prog);
    return prog;
  }

  program(name: string) {
    const p = this.programs.get(name);
    if (!p) throw new Error(`program belum dibuat: ${name}`);
    return p;
  }

  use(p: Program) {
    this.gl.useProgram(p.id);
  }

  /** target = null → canvas */
  bindTarget(t: Target | null, clear = false) {
    const gl = this.gl;
    if (t) {
      gl.bindFramebuffer(gl.FRAMEBUFFER, t.fb);
      gl.viewport(0, 0, t.w, t.h);
    } else {
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.viewport(0, 0, this.canvas.width, this.canvas.height);
    }
    if (clear) {
      gl.clearColor(0, 0, 0, 0);
      gl.clear(gl.COLOR_BUFFER_BIT);
    }
  }

  target(w: number, h: number, opts: TargetOpts = {}): Target {
    return new Target(
      Math.max(2, w | 0),
      Math.max(2, h | 0),
      opts.filter ?? this.gl.LINEAR,
      opts.internal ?? this.gl.RGBA8
    );
  }

  /** draw fullscreen triangle (VAO internal, tanpa attrib lain) */
  drawQuad() {
    const gl = this.gl;
    gl.bindVertexArray(this.quadVao);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
    gl.bindVertexArray(null);
  }
}

function header(gl: WebGL2RenderingContext, body: string) {
  return `#version 300 es
precision ${gl.getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.HIGH_FLOAT)?.precision ? 'highp' : 'mediump'} float;
precision highp int;
#define HAOCAM 1
${body}`;
}

function withLineNumbers(src: string) {
  return src
    .split('\n')
    .map((l, i) => `${String(i + 1).padStart(4, ' ')} | ${l}`)
    .slice(0, 400)
    .join('\n');
}


/**
 * GLSL_COMMON disisipkan ke SEMUA fragment shader (bukan vertex) supaya helper
 * numerik tersedia di tiap pass. Jangan menaruh kode yang hanya valid di fs sini.
 */
export const GLSL_COMMON = /* glsl */ `
#define PI 3.141592653589793
#define TAU 6.283185307179586

float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
float sat(vec3 c) { return max(max(c.r, c.g, c.b) - min(min(c.r, c.g, c.b)), 0.0); }

vec3 srgbToLinear(vec3 c) {
  return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}
vec3 linearToSrgb(vec3 c) {
  return mix(c * 12.92, 1.055 * pow(max(c, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}

// soft threshold / ramp yang dipakai hampir semua pass
float ramp(float x, float a, float b) { return clamp((x - a) / max(b - a, 1e-5), 0.0, 1.0); }
float smooth01(float x) { return x * x * (3.0 - 2.0 * x); }
float smoother01(float x) { return x * x * x * (x * (x * 6.0 - 15.0) + 10.0); }
float feather(float d, float px) { return 1.0 - ramp(d, 0.0, max(px, 1e-4)); }

vec3 mixPreserveLuma(vec3 base, vec3 tint, float amt, float lumaKeep) {
  vec3 c = mix(base, tint, amt);
  float l0 = luma(base), l1 = luma(c);
  return clamp(c * (1.0 + (l0 - l1) * lumaKeep), 0.0, 1.0);
}

// skin likelihood: kriteria Cb/Cr + luma (dipakai bilateral guide)
float skinness(vec3 rgb) {
  float r = rgb.r, g = rgb.g, b = rgb.b;
  float cb = 0.5 - 0.168736 * r - 0.331264 * g + 0.5 * b;
  float cr = 0.5 + 0.5 * r - 0.418688 * g - 0.081312 * b;
  float m = max(max(r, g), b) - min(min(r, g), b);
  float chroma = exp(-pow((cr - 0.522) / 0.10, 2.0)) * exp(-pow((cb - 0.397) / 0.092, 2.0));
  float cond = step(0.02, r - g) * step(0.0, r - b) * step(95.0 / 255.0, max(max(r, g), b)) * step(m, 0.35);
  return clamp(chroma * cond, 0.0, 1.0);
}

float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
float vnoise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  f = smooth01(f);
  float a = hash12(i), b = hash12(i + vec2(1, 0)), c = hash12(i + vec2(0, 1)), d = hash12(i + vec2(1, 1));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
`;
