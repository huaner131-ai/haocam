/**
 * FaceTracker — wrapper face-api.js (vendored, 100% lokal, tanpa CDN).
 *
 * Kenapa face-api (iBUG-68) alih-alih model 468 titik: paketnya berisi bobot
 * di dalam repo sehingga pipeline jalan offline; 68 titik + Delaunay sudah
 * cukup untuk semua efek beauty (kulit, mata, bibir, gigi) + mesh warp.
 * Interface-nya dibuat generik supaya tracker lain (mis. MediaPipe 468 pt)
 * bisa ditukar tanpa mengubah pipeline.
 */
import { N_POINTS, clamp, type FaceGeom } from './landmarks';
import { buildGeom } from './mesh';

export interface TrackerOpts {
  /** 'fast' = landmark tiny, 'sharp' = landmark full */
  accuracy: 'fast' | 'sharp';
  inputSize: number; // 320 / 416
  scoreThreshold: number;
  useExpressions: boolean;
}

export interface TrackerStatus {
  phase: 'idle' | 'loading' | 'ready' | 'error';
  message: string;
  progress: number; // 0..1
}

const VENDOR = './vendor/face-api.js';
const MODEL_DIR = './vendor/haocam-model';

let scriptPromise: Promise<void> | null = null;

function loadScript(): Promise<void> {
  if (scriptPromise) return scriptPromise;
  scriptPromise = new Promise<void>((resolve, reject) => {
    if (window.faceapi) return resolve();
    const el = document.createElement('script');
    el.src = VENDOR;
    el.async = true;
    el.onload = () => (window.faceapi ? resolve() : reject(new Error('faceapi global tidak ditemukan')));
    el.onerror = () => reject(new Error(`gagal memuat ${VENDOR}`));
    document.head.appendChild(el);
  });
  return scriptPromise;
}

export class FaceTracker {
  private ready = false;
  private busy = false;
  private wantRun = false;
  private smooth: Float32Array = new Float32Array(N_POINTS * 2);
  private box = new Float32Array(4);
  private vis = 0;
  private expr = { happy: 0, surprised: 0, neutral: 1 };
  private lastMs = 0;
  geom: FaceGeom | null = null;
  detectMs = 0;
  fps = 0;
  status: TrackerStatus = { phase: 'idle', message: 'model belum dimuat', progress: 0 };
  onStatus?: (s: TrackerStatus) => void;

  constructor(private opts: TrackerOpts) {}

  setOpts(o: Partial<TrackerOpts>) {
    Object.assign(this.opts, o);
  }

  async init() {
    try {
      this.emit({ phase: 'loading', message: 'memuat runtime deteksi…', progress: 0.1 });
      await loadScript();
      const fa = window.faceapi!;
      try {
        await fa.tf.setBackend('webgl');
      } catch {
        /* fallback ke cpu otomatis */
      }
      await fa.tf.ready();
      fa.tf.setProdMode(true);
      this.emit({ phase: 'loading', message: 'detector (tiny face)…', progress: 0.35 });
      await fa.nets.tinyFaceDetector.loadFromUri(MODEL_DIR);
      this.emit({ phase: 'loading', message: 'face mesh 68 titik…', progress: 0.7 });
      // muat DUA varian (tiny + full) supaya toggle akurasi di UI instan & tidak gagal
      await fa.nets.faceLandmark68TinyNet.loadFromUri(MODEL_DIR);
      await fa.nets.faceLandmark68Net.loadFromUri(MODEL_DIR);
      if (this.opts.useExpressions) {
        this.emit({ phase: 'loading', message: 'face expression (senyum/kedip)…', progress: 0.92 });
        await fa.nets.faceExpressionNet.loadFromUri(MODEL_DIR);
      }
      this.ready = true;
      this.emit({ phase: 'ready', message: 'face mesh aktif', progress: 1 });
    } catch (e) {
      this.ready = false;
      this.emit({ phase: 'error', message: (e as Error).message || 'model gagal dimuat', progress: 1 });
    }
  }

  private emit(s: TrackerStatus) {
    this.status = s;
    this.onStatus?.(s);
  }

  get isReady() {
    return this.ready;
  }

  /** dipanggil render loop; deteksi async & tidak memblokir frame */
  tick(src: HTMLVideoElement | HTMLCanvasElement | ImageBitmap, every: number, frame: number, mirrored: boolean) {
    if (!this.ready) return;
    if (this.busy) {
      this.wantRun = true;
      return;
    }
    if (every > 1 && frame % every !== 0) return;
    this.busy = true;
    this.wantRun = false;
    const t0 = performance.now();
    const fa = window.faceapi!;
    const options = new fa.TinyFaceDetectorOptions({
      inputSize: this.opts.inputSize,
      scoreThreshold: this.opts.scoreThreshold,
    });
    const chain = fa.detectAllFaces(src, options);
    const withLandmarks = chain.withFaceLandmarks(this.opts.accuracy === 'fast');
    const p = this.opts.useExpressions ? withLandmarks.withFaceExpressions() : Promise.resolve(null);
    p.then((res) => {
      const list = (res as unknown as FaceApiResult[] | null) ?? [];
      this.applyResult(list, src, mirrored);
    })
      .catch(() => {
        this.vis *= 0.6;
      })
      .finally(() => {
        this.detectMs = performance.now() - t0;
        const now = performance.now();
        if (this.lastMs) {
          const dt = now - this.lastMs;
          this.fps = this.fps * 0.7 + (1000 / Math.max(dt, 1)) * 0.3;
        }
        this.lastMs = now;
        this.busy = false;
        if (this.wantRun) this.tick(src, 1, frame, mirrored);
      });
  }

  private applyResult(
    list: FaceApiResult[] | null,
    src: HTMLVideoElement | HTMLCanvasElement | ImageBitmap,
    mirrored: boolean
  ) {
    const w = 'width' in src ? (src.width as number) : 1;
    const h = 'height' in src ? ((src as unknown as { height: number }).height as number) : 1;
    if (!w || !h) return;
    const best = list && list.length ? list.reduce((a, b) => (a.score >= b.score ? a : b)) : null;
    if (!best || best.landmarks.positions.length < N_POINTS) {
      this.vis = this.vis * 0.65;
      if (this.vis < 0.02) this.vis = 0;
      this.publish(mirrored);
      return;
    }
    const raw = new Float32Array(N_POINTS * 2);
    for (let i = 0; i < N_POINTS; i++) {
      const pt = best.landmarks.positions[i];
      raw[i * 2] = pt.x / w;
      raw[i * 2 + 1] = pt.y / h;
    }
    if (mirrored) for (let i = 0; i < N_POINTS; i++) raw[i * 2] = 1 - raw[i * 2];

    // EMA adaptif: makin besar lompatan → alpha makin tinggi (anti-lag)
    const prev = this.smooth;
    let sum = 0;
    for (let i = 0; i < N_POINTS * 2; i++) sum += Math.abs(raw[i] - prev[i]);
    const jump = sum / (N_POINTS * 2);
    const alpha = clamp(0.35 + jump * 14, 0.3, 0.95);
    for (let i = 0; i < N_POINTS * 2; i++) prev[i] = prev[i] ? lerp(prev[i], raw[i], alpha) : raw[i];
    if (this.vis < 1) this.vis = Math.min(1, this.vis + 0.25);

    const b = best.box;
    this.box[0] = b.x / w;
    this.box[1] = b.y / h;
    this.box[2] = b.width / w;
    this.box[3] = b.height / h;
    if (mirrored) this.box[0] = 1 - this.box[0] - this.box[2];

    if (best.expressions) {
      this.expr.happy = best.expressions.happy ?? 0;
      this.expr.surprised = best.expressions.surprised ?? 0;
      this.expr.neutral = best.expressions.neutral ?? 1;
    }
    this.publish(mirrored);
  }

  private publish(_mirrored: boolean) {
    const pts = new Float32Array(this.smooth);
    const box: [number, number, number, number] = [this.box[0], this.box[1], this.box[2], this.box[3]];
    this.geom = buildGeom(pts, this.vis, box, this.expr);
  }

  reset() {
    this.smooth = new Float32Array(N_POINTS * 2);
    this.vis = 0;
    this.geom = null;
  }
}

function lerp(a: number, b: number, t: number) {
  return a + (b - a) * t;
}
