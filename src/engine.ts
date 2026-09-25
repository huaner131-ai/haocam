/**
 * HaoEngine — orkestrasi sumber gambar + tracker + pipeline + overlay + recorder.
 *
 * Sumber yang didukung (sama seperti app kamera modern + jalur OBS):
 *   • kamera (getUserMedia, pilih device — termasuk "OBS Virtual Camera")
 *   • layar (getDisplayMedia — bisa dipakai buat nangkap window OBS)
 *   • file video/foto (drag & drop, untuk edit/beautify offline)
 */
import { FaceTracker } from './face/tracker';
import { Pipeline } from './fx/pipeline';
import { OverlayRenderer } from './fx/overlay';
import { Recorder, pickMime } from './fx/capture';
import { parseCube, type Lut } from './fx/lut';
import { QUALITY, type FxParams } from './fx/params';
import { clamp } from './face/landmarks';
import type { Store } from './state';

export type SourceKind = 'camera' | 'screen' | 'file';

export interface EngineStatus {
  source: SourceKind | null;
  label: string;
  error: string | null;
  fps: number;
  frame: number;
  face: boolean;
  faceCount: number;
  detectMs: number;
  drawMs: number;
  outW: number;
  outH: number;
  recording: boolean;
  recMs: number;
  mime: string | null;
  lut: string | null;
  model: { phase: string; message: string; progress: number };
}

export class HaoEngine {
  readonly video = document.createElement('video');
  readonly pipeline: Pipeline;
  readonly tracker: FaceTracker;
  readonly overlay = new OverlayRenderer();
  readonly recorder = new Recorder();
  status: EngineStatus = {
    source: null,
    label: 'siap',
    error: null,
    fps: 0,
    frame: 0,
    face: false,
    faceCount: 0,
    detectMs: 0,
    drawMs: 0,
    outW: 0,
    outH: 0,
    recording: false,
    recMs: 0,
    mime: null,
    lut: null,
    model: { phase: 'idle', message: 'model belum dimuat', progress: 0 },
  };
  onStatus?: (s: EngineStatus) => void;
  private stream: MediaStream | null = null;
  private micStream: MediaStream | null = null;
  private raf = 0;
  private frame = 0;
  private last = 0;
  private pendingPhoto: ((b: Blob) => void) | null = null;
  private fileUrl: string | null = null;
  private running = false;
  private flash = 0;
  private lastStatusPush = 0;
  private pendingLut: Lut | null = null;
  private openMouth = 0;

  constructor(private canvas: HTMLCanvasElement, private store: Store) {
    this.pipeline = new Pipeline(canvas);
    this.tracker = new FaceTracker({
      accuracy: store.live.accuracy,
      inputSize: 320,
      scoreThreshold: 0.45,
      useExpressions: true,
    });
    this.tracker.onStatus = () => this.pushStatus(true);
    Object.assign(this.status, { mime: pickMime()?.mime ?? null });
    this.video.muted = true;
    this.video.playsInline = true;
    this.video.autoplay = true;
    this.video.setAttribute('playsinline', '');
    this.recorder.onTick = (ms) => {
      this.status.recMs = ms;
      this.pushStatus();
    };
  }

  /* ── lifecycle ────────────────────────────────────────────── */
  async boot() {
    void this.tracker.init();
    this.running = true;
    const loop = (t: number) => {
      if (!this.running) return;
      this.raf = requestAnimationFrame(loop);
      this.frameStep(t);
    };
    this.raf = requestAnimationFrame(loop);
  }

  dispose() {
    this.running = false;
    cancelAnimationFrame(this.raf);
    this.stopStream();
    this.pipeline.dispose();
  }

  private stopStream() {
    this.tracker.reset();
    this.stream?.getTracks().forEach((t) => t.stop());
    this.stream = null;
    if (this.fileUrl) {
      URL.revokeObjectURL(this.fileUrl);
      this.fileUrl = null;
    }
  }

  async listCameras(): Promise<MediaDeviceInfo[]> {
    try {
      const devs = await navigator.mediaDevices.enumerateDevices();
      return devs.filter((d) => d.kind === 'videoinput');
    } catch {
      return [];
    }
  }

  async startCamera(deviceId?: string, facing: 'user' | 'environment' = 'user') {
    this.stopStream();
    try {
      const constraints: MediaStreamConstraints = {
        video: deviceId
          ? { deviceId: { exact: deviceId }, width: { ideal: 1920 }, height: { ideal: 1080 }, frameRate: { ideal: 60 } }
          : { facingMode: facing, width: { ideal: 1920 }, height: { ideal: 1080 }, frameRate: { ideal: 30 } },
        audio: false,
      };
      const stream = await navigator.mediaDevices.getUserMedia(constraints);
      this.stream = stream;
      this.video.srcObject = stream;
      await this.video.play().catch(() => undefined);
      const track = stream.getVideoTracks()[0];
      const caps = (track?.getCapabilities?.() ?? {}) as MediaTrackCapabilities & { width?: number; height?: number };
      this.status.source = 'camera';
      this.status.label =
        (track?.label || 'kamera') + (caps.width ? ` · ${caps.width}×${caps.height ?? '?'}` : '');
      this.status.error = null;
      this.store.setParam('mirror', facing === 'user' ? 1 : 0);
      this.pushStatus(true);
    } catch (e) {
      this.status.error = humanMediaError(e as Error);
      this.status.source = null;
      this.status.label = 'kamera gagal';
      this.pushStatus(true);
      throw e;
    }
  }

  async startScreen() {
    if (!navigator.mediaDevices.getDisplayMedia) throw new Error('getDisplayMedia tidak tersedia');
    this.stopStream();
    const stream = await navigator.mediaDevices.getDisplayMedia({ video: { frameRate: 30 }, audio: false });
    this.stream = stream;
    this.video.srcObject = stream;
    await this.video.play().catch(() => undefined);
    stream.getVideoTracks()[0].addEventListener('ended', () => this.stopStream());
    this.status.source = 'screen';
    this.status.label = 'layar / jendela (bisa OBS Output)';
    this.status.error = null;
    this.pushStatus(true);
  }

  async useFile(file: File) {
    this.stopStream();
    const url = URL.createObjectURL(file);
    this.fileUrl = url;
    if (file.type.startsWith('video/')) {
      this.video.srcObject = null;
      this.video.src = url;
      this.video.loop = true;
      this.video.muted = true;
      await new Promise<void>((res) => {
        const done = () => res();
        if (this.video.readyState >= 1) done();
        else this.video.onloadedmetadata = done;
      });
      await this.video.play().catch(() => undefined);
    } else {
      const img = new Image();
      img.src = url;
      await new Promise<void>((res, rej) => {
        img.onload = () => res();
        img.onerror = () => rej(new Error('gambar tidak terbaca'));
      });
      const cv = document.createElement('canvas');
      const long = Math.max(img.naturalWidth, img.naturalHeight);
      const k = long > 1920 ? 1920 / long : 1;
      cv.width = Math.round(img.naturalWidth * k);
      cv.height = Math.round(img.naturalHeight * k);
      cv.getContext('2d')!.drawImage(img, 0, 0, cv.width, cv.height);
      this.still = cv;
    }
    this.status.source = 'file';
    this.status.label = file.type.startsWith('video/') ? `video: ${file.name}` : `foto: ${file.name}`;
    this.status.error = null;
    this.pushStatus(true);
  }

  private still: HTMLCanvasElement | null = null;

  /* ── frame loop ───────────────────────────────────────────── */
  private frameStep(now: number) {
    const st = this.store.live;
    const p = st.params;
    const q = QUALITY[st.quality];
    const src = this.currentSource();
    if (!src || srcW(src) < 8) {
      this.idleDraw();
      return;
    }
    const w = srcW(src);
    const h = srcH(src);

    const rect = this.canvas.getBoundingClientRect();
    const cssW = Math.max(160, Math.round(rect.width));
    const cssH = Math.max(160, Math.round(rect.height));
    const aspect = p.aspectRatio > 0 ? p.aspectRatio : 0; // w/h
    const resized = this.pipeline.resize(cssW, cssH, aspect, q.scale, q.maskScale);
    if (resized) this.overlay.resize(this.pipeline.width, this.pipeline.height);
    if (this.pendingLut) {
      this.pipeline.setLut(this.pendingLut);
      this.pendingLut = null;
    }

    // tracker: pakai still-video hanya tiap N frame
    this.tracker.tick(src as HTMLVideoElement, q.trackEvery, this.frame, p.mirror > 0.5);
    const geom = this.tracker.geom;

    // trigger ekspresi → mouth openness dari landmark
    if (geom) {
      const open = dist2(geom.pts, 63, 67) / Math.max(dist2(geom.pts, 48, 54), 1e-4);
      this.openMouth = clamp((open - 0.08) / 0.34, 0, 1) * geom.vis;
    } else this.openMouth *= 0.8;

    this.overlay.draw({
      geom,
      params: p,
      time: now / 1000,
      smile: geom ? geom.happy : 0,
      openMouth: this.openMouth,
      blink: geom ? geom.blink : 0,
      showMesh: st.showMesh,
    });

    this.pipeline.render({
      source: src as HTMLVideoElement,
      srcW: w,
      srcH: h,
      geom,
      params: p,
      overlay: this.overlay.canvas,
      time: now / 1000,
      compare: st.compare,
      flash: this.flash,
      warpStrength: 1,
      autoExp: 0,
    });

    if (this.flash > 0) this.flash = Math.max(0, this.flash - 0.08);
    if (this.pendingPhoto) {
      const cb = this.pendingPhoto;
      this.pendingPhoto = null;
      this.canvasToBlob().then(cb);
    }

    if (this.last) {
      const dt = now - this.last;
      this.status.fps = this.status.fps * 0.9 + (1000 / Math.max(dt, 1)) * 0.1;
    }
    this.last = now;
    this.status.frame = this.frame++;
    this.status.face = !!geom && geom.vis > 0.1;
    this.status.detectMs = this.tracker.detectMs;
    this.status.drawMs = this.pipeline.stats.drawMs;
    this.status.outW = this.pipeline.width;
    this.status.outH = this.pipeline.height;
    this.status.recording = this.recorder.isRecording;
    this.status.lut = this.pipeline.lutName;
    this.status.model = { ...this.tracker.status };
    this.pushStatus();
  }

  private idleDraw() {
    const st = this.store.live;
    const rect = this.canvas.getBoundingClientRect();
    const q = QUALITY[st.quality];
    const aspect = st.params.aspectRatio > 0 ? st.params.aspectRatio : 0;
    if (this.pipeline.resize(Math.max(160, rect.width), Math.max(160, rect.height), aspect, q.scale, q.maskScale)) {
      this.overlay.resize(this.pipeline.width, this.pipeline.height);
    }
    const gl = this.pipeline.gfx.gl;
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.clearColor(0.03, 0.03, 0.045, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
    this.pushStatus();
  }

  currentSource(): HTMLVideoElement | HTMLCanvasElement | null {
    if (this.stream) return this.video;
    if (this.still) return this.still;
    if (this.video.src && this.video.readyState >= 2) return this.video;
    return this.video.readyState >= 2 ? this.video : null;
  }

  /* ── aksi UI ──────────────────────────────────────────────── */
  snapshot(): Promise<Blob | null> {
    return new Promise((res) => {
      this.flash = 1;
      this.pendingPhoto = (blob) => res(blob);
    });
  }

  private async canvasToBlob(): Promise<Blob> {
    try {
      return await new Promise<Blob>((res, rej) =>
        this.canvas.toBlob((b) => (b ? res(b) : rej(new Error('toBlob gagal'))), 'image/png')
      );
    } catch {
      return new Blob([], { type: 'image/png' });
    }
  }

  /** ambil foto (blob PNG) — download/share ditangani UI */
  async capturePhoto(): Promise<Blob | null> {
    return this.snapshot();
  }

  get recording() {
    return this.recorder.isRecording;
  }
  get recMs() {
    return this.recorder.elapsed;
  }

  async startRecord(): Promise<{ mime: string }> {
    if (this.recorder.isRecording) throw new Error('sudah merekam');
    if (this.store.live.audio && !this.micStream) {
      try {
        this.micStream = await navigator.mediaDevices.getUserMedia({
          audio: { echoCancellation: true, noiseSuppression: true },
        });
      } catch {
        this.micStream = null;
      }
    }
    const rec = await this.recorder.start(this.canvas, {
      fps: this.store.live.recordFps,
      videoBitsPerSecond: Math.round(this.store.live.bitrateMbps * 1_000_000),
      withAudio: this.store.live.audio,
      micStream: this.micStream,
    });
    this.status.recording = true;
    this.status.recMs = 0;
    this.pushStatus(true);
    return rec;
  }

  async stopRecord(): Promise<{ blob: Blob; ext: string; ms: number }> {
    const out = await this.recorder.stop();
    this.status.recording = false;
    this.pushStatus(true);
    return out;
  }

  async loadCubeFromText(text: string, name: string) {
    const lut = parseCube(text, name);
    if (!lut) throw new Error('file .cube tidak valid');
    this.pendingLut = lut;
    this.store.setParam('lutMode', 1);
    this.pushStatus(true);
    return lut.size;
  }
  clearLut() {
    this.pendingLut = null;
    this.pipeline.setLut(null);
    this.store.setParam('lutMode', 0);
    this.pushStatus(true);
  }

  private pushStatus(force = false) {
    const now = performance.now();
    if (!force && now - this.lastStatusPush < 240) return;
    this.lastStatusPush = now;
    this.onStatus?.({ ...this.status });
  }
}

function srcW(s: HTMLVideoElement | HTMLCanvasElement) {
  return s instanceof HTMLVideoElement ? s.videoWidth : s.width;
}
function srcH(s: HTMLVideoElement | HTMLCanvasElement) {
  return s instanceof HTMLVideoElement ? s.videoHeight : s.height;
}
function dist2(pts: Float32Array, a: number, b: number) {
  return Math.hypot(pts[a * 2] - pts[b * 2], pts[a * 2 + 1] - pts[b * 2 + 1]);
}
function humanMediaError(e: Error) {
  const n = (e as { name?: string }).name;
  if (n === 'NotAllowedError') return 'akses kamera ditolak — izinkan camera di browser';
  if (n === 'NotFoundError') return 'kamera tidak ditemukan (coba pilih device lain / OBS Virtual Camera)';
  if (n === 'NotReadableError') return 'kamera dipakai app lain (zoom/meet/obs)';
  return e.message || 'gagal membuka sumber';
}

export type { FxParams };
