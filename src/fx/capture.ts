/**
 * Capture — rekam video (canvas.captureStream + audio mic) & foto,
 * plus auto-download / share. Persis alur "share" app kamera.
 */

export interface RecordOptions {
  fps: number;
  videoBitsPerSecond: number;
  withAudio: boolean;
  micStream?: MediaStream | null;
}

export interface Recording {
  mime: string;
  ext: string;
}

const CANDIDATES: { mime: string; ext: string }[] = [
  { mime: 'video/mp4;codecs=avc1.640028,mp4a.40.2', ext: 'mp4' },
  { mime: 'video/mp4;codecs=avc1.42E01E', ext: 'mp4' },
  { mime: 'video/mp4', ext: 'mp4' },
  { mime: 'video/webm;codecs=vp9,opus', ext: 'webm' },
  { mime: 'video/webm;codecs=vp8,opus', ext: 'webm' },
  { mime: 'video/webm', ext: 'webm' },
];

export function pickMime(): Recording | null {
  if (typeof MediaRecorder === 'undefined') return null;
  for (const c of CANDIDATES) if (MediaRecorder.isTypeSupported(c.mime)) return c;
  return null;
}

export class Recorder {
  private mr: MediaRecorder | null = null;
  private chunks: Blob[] = [];
  private started = 0;
  onTick?: (ms: number) => void;
  private timer = 0;

  get supported() {
    return !!pickMime();
  }
  get isRecording() {
    return this.mr?.state === 'recording';
  }
  get elapsed() {
    return this.started ? performance.now() - this.started : 0;
  }

  async start(canvas: HTMLCanvasElement, opts: RecordOptions): Promise<Recording> {
    if (this.isRecording) throw new Error('sudah merekam');
    const rec = pickMime();
    if (!rec) throw new Error('browser tidak mendukung MediaRecorder video');
    const fps = Math.max(1, Math.min(60, Math.round(opts.fps)));
    const stream = canvas.captureStream(fps);
    if (opts.withAudio && opts.micStream) {
      for (const tr of opts.micStream.getAudioTracks()) stream.addTrack(tr);
    }
    const mr = new MediaRecorder(stream, {
      mimeType: rec.mime,
      videoBitsPerSecond: opts.videoBitsPerSecond,
      audioBitsPerSecond: 128_000,
    });
    this.chunks = [];
    mr.ondataavailable = (e) => {
      if (e.data && e.data.size > 0) this.chunks.push(e.data);
    };
    mr.start(1000);
    this.mr = mr;
    void stream;
    this.started = performance.now();
    window.clearInterval(this.timer);
    this.timer = window.setInterval(() => this.onTick?.(this.elapsed), 200);
    return rec;
  }

  stop(): Promise<{ blob: Blob; ext: string; ms: number }> {
    return new Promise((resolve, reject) => {
      const mr = this.mr;
      if (!mr) return reject(new Error('tidak sedang merekam'));
      const ext = (mr.mimeType.match(/video\/(\w+)/) ?? [])[1] ?? 'webm';
      const ms = this.elapsed;
      mr.onstop = () => {
        this.mr = null;
        window.clearInterval(this.timer);
        const blob = new Blob(this.chunks, { type: mr.mimeType });
        this.chunks = [];
        resolve({ blob, ext, ms });
      };
      mr.onerror = (e) => reject(e);
      try {
        mr.requestData?.();
      } catch {
        /* noop */
      }
      mr.stop();
    });
  }

  abort() {
    try {
      this.mr?.stop();
    } catch {
      /* noop */
    }
  }
}

export function download(blob: Blob, filename: string) {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 4000);
}

export async function copyBlob(blob: Blob) {
  try {
    if (typeof ClipboardItem !== 'undefined' && navigator.clipboard?.write) {
      await navigator.clipboard.write([new ClipboardItem({ [blob.type]: blob })]);
      return true;
    }
  } catch {
    /* clipboard video sering tidak diizinkan */
  }
  return false;
}

export async function nativeShare(blob: Blob, title: string, ext: string) {
  const file = new File([blob], `${title}.${ext}`, { type: blob.type || 'application/octet-stream' });
  const nav = navigator as Navigator & { canShare?: (d: unknown) => boolean };
  if (nav.share && nav.canShare?.({ files: [file] })) {
    try {
      await nav.share({ files: [file], title });
      return true;
    } catch {
      return false;
    }
  }
  return false;
}

export function stampName(prefix: string, ext: string) {
  const d = new Date();
  const p = (v: number) => String(v).padStart(2, '0');
  return `${prefix}-${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}.${ext}`;
}
