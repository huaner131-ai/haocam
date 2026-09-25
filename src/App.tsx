import { useCallback, useEffect, useRef, useState } from 'react';
import { HaoEngine, type EngineStatus } from './engine';
import { store } from './state';
import { BeautyPanel, FilterPanel, FramePanel, LensPanel, ProPanel } from './ui/panels';
import { SourceModal } from './ui/SourceModal';
import { Chip, fmtMs, useApp, useToast } from './ui/controls';
import { ASPECTS, type Aspect } from './fx/params';
import { copyBlob, download, nativeShare, stampName } from './fx/capture';

type Tab = 'beauty' | 'filter' | 'lens' | 'frame' | 'pro';

export function App() {
  const st = useApp();
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const engineRef = useRef<HaoEngine | null>(null);
  const [status, setStatus] = useState<EngineStatus | null>(null);
  const [tab, setTab] = useState<Tab>('beauty');
  const [settings, setSettings] = useState(false);
  const [mode, setMode] = useState<'photo' | 'video'>('photo');
  const [clip, setClip] = useState<{ url: string; kind: 'image' | 'video'; ext: string; ms?: number } | null>(null);
  const { msg, toast } = useToast();
  const [fatal, setFatal] = useState<string | null>(null);
  const pointers = useRef(new Map<number, { x: number; y: number }>());
  const pinch = useRef(0);
  const [drag, setDrag] = useState(false);

  /* ── boot engine ─────────────────────────────────────────── */
  useEffect(() => {
    const canvas = canvasRef.current!;
    let eng: HaoEngine;
    try {
      eng = new HaoEngine(canvas, store);
    } catch (e) {
      setFatal((e as Error).message);
      return;
    }
    engineRef.current = eng;
    eng.onStatus = setStatus;
    document.body.appendChild(eng.video);
    eng.video.className = 'src-hidden';
    eng.boot();
    void eng.startCamera().catch(() => undefined);
    return () => {
      eng!.dispose();
      eng.video.remove();
      engineRef.current = null;
    };
  }, []);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.target instanceof HTMLInputElement) return;
      if (e.code === 'Space') {
        e.preventDefault();
        void shoot();
      }
      if (e.key.toLowerCase() === 'r') store.setParam('mirror', st.params.mirror > 0.5 ? 0 : 1);
      if (e.key.toLowerCase() === 'c') store.set('compare', 0.5);
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [st.params.mirror]);

  /* ── aksi ────────────────────────────────────────────────── */
  const shoot = useCallback(async () => {
    const eng = engineRef.current;
    if (!eng) return;
    if (eng.recording) {
      try {
        const out = await eng.stopRecord();
        download(out.blob, stampName('haocam', out.ext));
        setClip({ url: URL.createObjectURL(out.blob), kind: 'video', ext: out.ext, ms: out.ms });
        toast(`tersimpan · ${(out.blob.size / 1e6).toFixed(1)} MB`);
      } catch (e) {
        toast((e as Error).message);
      }
      return;
    }
    if (mode === 'photo') {
      const blob = await eng.capturePhoto();
      if (!blob) return toast('gagal ambil foto');
      download(blob, stampName('haocam', 'png'));
      setClip({ url: URL.createObjectURL(blob), kind: 'image', ext: 'png' });
      toast('foto tersimpan');
      return;
    }
    try {
      const rec = await eng.startRecord();
      toast(`rekam ${rec.mime.split(';')[0]} · ${store.live.recordFps}fps ${store.live.bitrateMbps}Mbps`);
    } catch (e) {
      toast((e as Error).message);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [mode, toast]);

  const cycleAspect = () => {
    const i = ASPECTS.findIndex((a) => a.id === st.params.aspect);
    const next = ASPECTS[(i + 1) % ASPECTS.length];
    store.setParam('aspect', next.id as Aspect);
  };

  /* pinch zoom + hold-compare */
  const onPointerDown = (e: React.PointerEvent) => {
    pointers.current.set(e.pointerId, { x: e.clientX, y: e.clientY });
    if (pointers.current.size === 1 && e.pointerType !== 'mouse') {
      store.set('compare', 0.42);
    }
  };
  const onPointerMove = (e: React.PointerEvent) => {
    if (!pointers.current.has(e.pointerId)) return;
    pointers.current.set(e.pointerId, { x: e.clientX, y: e.clientY });
    const pts = [...pointers.current.values()];
    if (pts.length === 2) {
      const d = Math.hypot(pts[0].x - pts[1].x, pts[0].y - pts[1].y);
      if (pinch.current) {
        const z = Math.max(1, Math.min(2.6, st.params.zoom * (d / pinch.current)));
        store.setParam('zoom', z);
      }
      pinch.current = d;
    }
  };
  const endPointer = (e: React.PointerEvent) => {
    pointers.current.delete(e.pointerId);
    if (pointers.current.size < 2) pinch.current = 0;
    if (pointers.current.size === 0) store.set('compare', 0);
  };

  const onDrop = (e: React.DragEvent) => {
    e.preventDefault();
    setDrag(false);
    const f = e.dataTransfer.files?.[0];
    if (f) engineRef.current?.useFile(f).then(() => toast('source: ' + f.name)).catch((x) => toast((x as Error).message));
  };

  const ar0 = st.params.aspectRatio;
  const ar = ar0 > 0 ? `${ar0} / 1` : 'auto';
  const trackerMsg = engineRef.current?.tracker.status;

  if (fatal)
    return (
      <div className="stage-hint" style={{ position: 'fixed', inset: 0 }}>
        <div>
          <div className="big">WebGL2 tidak siap</div>
          <div className="note" style={{ marginTop: 8, maxWidth: 420 }}>{fatal}</div>
          <div className="note" style={{ marginTop: 8 }}>
            Aktifkan hardware acceleration (chrome://gpu) atau buka di browser lain.
          </div>
        </div>
      </div>
    );

  return (
    <div className="app">
      <header className="top">
        <span className="brand">HAOCAM</span>
        <div className="chips">
          <span className="chip">
            <b>{Math.round(status?.fps ?? 0)}</b> fps
          </span>
          <span className="chip">
            <b>{status?.outW ?? 0}×{status?.outH ?? 0}</b>
          </span>
          <span className="chip" title="waktu deteksi wajah / render">
            track <b>{(status?.detectMs ?? 0).toFixed(1)}</b>ms · gpu <b>{(status?.drawMs ?? 0).toFixed(1)}</b>ms
          </span>
          <span className={`chip ${status?.face ? '' : 'warn'}`}>{status?.face ? '🙂 face lock' : '🚫 tak terdeteksi'}</span>
          <span
            className={`chip ${status?.model.phase === 'error' ? 'warn' : status?.model.phase === 'ready' ? '' : 'warn'}`}
            title="face tracker (face-api, lokal)"
          >
            {status?.model.phase === 'ready'
              ? '🧠 mesh 68pt'
              : status?.model.phase === 'error'
                ? `⚠ ${status.model.message}`
                : `⏳ ${status?.model.message ?? 'memuat model'}`}
          </span>
          {st.quality !== 'ultra' && <span className="chip">{st.quality === 'power' ? 'hemat' : 'seimbang'}</span>}
          {status?.lut && <span className="chip live">LUT {status.lut}</span>}
          <button className="iconbtn" onClick={() => setSettings(true)} title="sumber & kualitas">
            ⚙
          </button>
        </div>
      </header>

      <main
        className={`stage ${drag ? 'dropzone over' : ''}`}
        onDragOver={(e) => {
          e.preventDefault();
          setDrag(true);
        }}
        onDragLeave={() => setDrag(false)}
        onDrop={onDrop}
      >
        <div className="frame">
          <canvas
            ref={canvasRef}
            className="view"
            style={{ ['--ar' as string]: ar }}
            onPointerDown={onPointerDown}
            onPointerMove={onPointerMove}
            onPointerUp={endPointer}
            onPointerCancel={endPointer}
            onWheel={(e) => {
              const z = Math.max(1, Math.min(2.6, store.live.params.zoom + (e.deltaY > 0 ? 0.05 : -0.05)));
              store.setParam('zoom', z);
            }}
          />
          {status?.recording && (
            <div className="rec-badge">
              <span className="dot" /> REC {fmtMs(status.recMs)}
            </div>
          )}
          {(!status || !status.source) && (
            <div className="stage-hint">
              <div>
                <div className="big">Aktifkan sumber gambar</div>
                <div style={{ marginTop: 6, maxWidth: 360 }}>
                  {status?.error ??
                    (trackerMsg?.phase === 'loading'
                      ? `memuat model: ${trackerMsg.message}`
                      : 'kamera diblokir / tidak ada — pilih device, layar, atau lempar file video ke sini')}
                </div>
                <button className="btn primary" style={{ marginTop: 10 }} onClick={() => setSettings(true)}>
                  pilih sumber
                </button>
              </div>
            </div>
          )}
          <div className="rail l">
            <button
              className="iconbtn"
              title="mirroring (R)"
              onClick={() => store.setParam('mirror', st.params.mirror > 0.5 ? 0 : 1)}
            >
              ⇋
            </button>
            <button
              className="iconbtn"
              title="tahan canvas = before/after"
              onClick={() => store.set('compare', st.compare > 0 ? 0 : 0.42)}
            >
              ◧
            </button>
            <button
              className="iconbtn"
              title="beautify on/off"
              onClick={() => store.setMaster(st.masterBeauty > 0.5 ? 0 : 1)}
            >
              ✨
            </button>
          </div>
          <div className="rail r">
            <button className="iconbtn" title="rasio" onClick={cycleAspect}>
              {st.params.aspect === 'full' ? '⛶' : st.params.aspect}
            </button>
            <button className="iconbtn" title="rotate 90°" onClick={() => store.setParam('rotate', (st.params.rotate + 90) % 360)}>
              ⟳
            </button>
            <button
              className="iconbtn"
              title="zoom"
              onClick={() => store.setParam('zoom', st.params.zoom > 1.05 ? 1 : 1.6)}
            >
              ⌕
            </button>
          </div>
        </div>
      </main>

      <div className="bottom">
        <div className="shutterbar">
          <div className="row" style={{ margin: 0, gap: 4 }}>
            <button className={`mode-btn ${mode === 'photo' ? 'on' : ''}`} onClick={() => setMode('photo')}>
              Foto
            </button>
            <button className={`mode-btn ${mode === 'video' ? 'on' : ''}`} onClick={() => setMode('video')}>
              Video
            </button>
          </div>
          <button
            className={`shutter ${status?.recording ? 'rec' : ''}`}
            onClick={() => void shoot()}
            title={mode === 'photo' ? 'ambil foto (spasi)' : status?.recording ? 'hentikan rekam' : 'mulai rekam'}
          >
            <i />
          </button>
          <div className="row" style={{ margin: 0, gap: 6, alignItems: 'center' }}>
            {clip && (
              <button
                className="thumb"
                style={{ backgroundImage: clip.kind === 'image' ? `url(${clip.url})` : undefined }}
                onClick={() => setClip({ ...clip })}
                title="hasil terakhir"
              >
                {clip.kind === 'video' ? '🎬' : ''}
              </button>
            )}
            <button
              className="iconbtn"
              title="tukar kamera"
              onClick={() => {
                const eng = engineRef.current;
                if (!eng) return;
                const front = st.params.mirror > 0.5;
                eng.listCameras().then((d) => {
                  if (d.length > 1) eng.startCamera(d[front ? 1 : 0].deviceId, front ? 'environment' : 'user');
                  else store.setParam('mirror', front ? 0 : 1);
                });
              }}
            >
              🔄
            </button>
          </div>
        </div>

        <section className="sheet">
          <nav className="tabs">
            {(
              [
                ['beauty', '✨ Beauty'],
                ['filter', '🎨 Filter'],
                ['lens', '🐱 Lens AR'],
                ['frame', '🖼 Frame'],
                ['pro', '🎚 Pro'],
              ] as [Tab, string][]
            ).map(([id, label]) => (
              <button key={id} className={`tab ${tab === id ? 'on' : ''}`} onClick={() => setTab(id)}>
                {label}
              </button>
            ))}
            <button className="tab" onClick={() => store.reset()} title="reset semua parameter">
              ↺
            </button>
          </nav>
          <div className="tabbody">
            {tab === 'beauty' && <BeautyPanel st={st} />}
            {tab === 'filter' && <FilterPanel st={st} />}
            {tab === 'lens' && <LensPanel st={st} />}
            {tab === 'frame' && <FramePanel st={st} />}
            {tab === 'pro' && <ProPanel st={st} />}
          </div>
        </section>
      </div>

      {settings && <SourceModal engine={engineRef.current!} onClose={() => setSettings(false)} />}

      {clip && (
        <div className="scrim" onClick={() => setClip(null)}>
          <div className="modal" onClick={(e) => e.stopPropagation()}>
            <h3>{clip.kind === 'image' ? 'Foto terakhir' : `Rekaman ${clip.ms ? fmtMs(clip.ms) : ''}`}</h3>
            {clip.kind === 'image' ? (
              <img src={clip.url} alt="" style={{ width: '100%', borderRadius: 14 }} />
            ) : (
              <video src={clip.url} controls loop style={{ width: '100%', borderRadius: 14 }} />
            )}
            <div className="row" style={{ justifyContent: 'flex-end' }}>
              <Chip
                onClick={async () => {
                  const ok = await copyBlob(await (await fetch(clip.url)).blob());
                  toast(ok ? 'disalin ke clipboard' : 'clipboard ditolak browser');
                }}
              >
                Copy
              </Chip>
              <Chip
                onClick={async () => {
                  const blob = await (await fetch(clip.url)).blob();
                  const ok = await nativeShare(blob, 'haocam', clip.ext);
                  if (!ok) toast('share tidak didukung di browser ini — sudah diunduh');
                }}
              >
                Share
              </Chip>
              <Chip
                onClick={async () => {
                  const blob = await (await fetch(clip.url)).blob();
                  download(blob, stampName('haocam', clip.ext));
                }}
              >
                Unduh
              </Chip>
            </div>
          </div>
        </div>
      )}

      {msg && <div className="toast">{msg}</div>}
    </div>
  );
}
