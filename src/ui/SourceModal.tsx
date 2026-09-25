import { useEffect, useRef, useState } from 'react';
import type { HaoEngine } from '../engine';
import { store } from '../state';
import { useApp } from './controls';
import { ASPECTS } from '../fx/params';

/**
 * Panel sumber: kamera (device apa pun, termasuk OBS Virtual Camera), layar,
 * atau file video/foto — plus setting rekam & kualitas render.
 */
export function SourceModal({ engine, onClose }: { engine: HaoEngine; onClose: () => void }) {
  const st = useApp();
  const [devices, setDevices] = useState<MediaDeviceInfo[]>([]);
  const [err, setErr] = useState<string | null>(null);
  const fileRef = useRef<HTMLInputElement>(null);
  const lutRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    let alive = true;
    engine.listCameras().then((d) => alive && setDevices(d));
    return () => {
      alive = false;
    };
  }, [engine]);

  const pick = async (deviceId?: string) => {
    try {
      setErr(null);
      await engine.startCamera(deviceId);
      const d = await engine.listCameras();
      setDevices(d);
    } catch (e) {
      setErr((e as Error).message);
    }
  };

  return (
    <div className="scrim" onClick={onClose}>
      <div className="modal" onClick={(e) => e.stopPropagation()}>
        <h3>Sumber &amp; kualitas</h3>

        <div className="field">
          <label>Kamera / device input</label>
          <div className="list">
            <button onClick={() => pick(undefined)}>
              <span>🎥 Kamera default</span>
              <span style={{ color: '#8b8ba3' }}>{st.params.mirror > 0.5 ? 'depan' : 'belakang'}</span>
            </button>
            {devices.map((d, i) => (
              <button key={d.deviceId || i} onClick={() => pick(d.deviceId)}>
                <span>▸ {d.label || `Kamera ${i + 1}`}</span>
                {/obs|virtual/i.test(d.label) && <span style={{ color: '#8ad6a0' }}>OBS</span>}
              </button>
            ))}
            <button
              onClick={() => {
                engine.startScreen().catch((e) => setErr((e as Error).message));
              }}
            >
              <span>🖥️ Layar / jendela (getDisplayMedia)</span>
            </button>
            <button onClick={() => fileRef.current?.click()}>
              <span>📁 Buka video / foto dari disk</span>
            </button>
          </div>
          <input
            ref={fileRef}
            type="file"
            accept="video/*,image/*"
            style={{ display: 'none' }}
            onChange={(e) => {
              const f = e.target.files?.[0];
              if (f) engine.useFile(f).catch((x) => setErr((x as Error).message));
            }}
          />
          {err && <span className="note" style={{ color: '#ffb3b3' }}>{err}</span>}
          <span className="note">
            Pengunjung: install plugin beauty di OBS → aktifkan filter-nya → pilih <code>OBS Virtual Camera</code>{' '}
            di daftar atas. Semua proses OBS (beauty, LUT, 3D) masuk sebagai frame mentah HaoCam, lalu di- beautify
            &amp; di-grade ulang di GPU.
          </span>
        </div>

        <div className="field">
          <label>Face tracker</label>
          <div className="row" style={{ margin: 0 }}>
            {(['fast', 'sharp'] as const).map((a) => (
              <button
                key={a}
                className={`pill ${st.accuracy === a ? 'on' : ''}`}
                onClick={() => {
                  store.set('accuracy', a);
                  engine.tracker.setOpts({ accuracy: a });
                }}
              >
                {a === 'fast' ? 'Cepat (68-tiny)' : 'Tajam (68-full)'}
              </button>
            ))}
            <button className={`pill ${st.showMesh ? 'on' : ''}`} onClick={() => store.set('showMesh', !st.showMesh)}>
              overlay mesh
            </button>
            <input
              type="range"
              min={256}
              max={416}
              step={32}
              value={320}
              style={{ width: 90 }}
              onChange={(e) => engine.tracker.setOpts({ inputSize: parseInt(e.target.value, 10) })}
              title="input size detektor"
            />
          </div>
        </div>

        <div className="field">
          <label>Kualitas render</label>
          <div className="row" style={{ margin: 0 }}>
            {(['power', 'balanced', 'ultra'] as const).map((q) => (
              <button key={q} className={`pill ${st.quality === q ? 'on' : ''}`} onClick={() => store.set('quality', q)}>
                {q === 'power' ? 'Hemat' : q === 'balanced' ? 'Seimbang' : 'Ultra'}
              </button>
            ))}
          </div>
        </div>

        <div className="field">
          <label>Rekaman</label>
          <div className="row" style={{ gap: 10, margin: 0, alignItems: 'center' }}>
            <span className="note">fps</span>
            <input
              type="number"
              min={12}
              max={60}
              value={st.recordFps}
              style={{ width: 74 }}
              onChange={(e) => store.set('recordFps', Math.max(12, Math.min(60, +e.target.value || 30)))}
            />
            <span className="note">Mbps</span>
            <input
              type="number"
              min={1}
              max={40}
              step={0.5}
              value={st.bitrateMbps}
              style={{ width: 74 }}
              onChange={(e) => store.set('bitrateMbps', Math.max(1, Math.min(40, +e.target.value || 8)))}
            />
            <button className={`pill ${st.audio ? 'on' : ''}`} onClick={() => store.set('audio', !st.audio)}>
              🎙️ audio {st.audio ? 'on' : 'off'}
            </button>
          </div>
        </div>

        <div className="field">
          <label>LUT (.cube) — grading presisi seperti preset LUT di app kamera</label>
          <div className="row" style={{ margin: 0 }}>
            <button
              className="pill"
              onClick={() => lutRef.current?.click()}
            >
              {engine.status.lut ? `ganti: ${engine.status.lut}` : 'muat .cube'}
            </button>
            {engine.status.lut && (
              <button className="pill" onClick={() => engine.clearLut()}>
                lepas LUT
              </button>
            )}
            <input
              ref={lutRef}
              type="file"
              accept=".cube,text/plain"
              style={{ display: 'none' }}
              onChange={async (e) => {
                const f = e.target.files?.[0];
                if (!f) return;
                try {
                  const size = await engine.loadCubeFromText(await f.text(), f.name);
                  setErr(null);
                  console.log('LUT dimuat', size);
                } catch (x) {
                  setErr((x as Error).message);
                }
              }}
            />
            <span className="note">mix {Math.round(st.params.lutStrength * 100)}%</span>
          </div>
        </div>

        <div className="field">
          <label>Rasio</label>
          <div className="row" style={{ margin: 0 }}>
            {ASPECTS.map((a) => (
              <button
                key={a.id}
                className={`pill ${st.params.aspect === a.id ? 'on' : ''}`}
                onClick={() => store.setParam('aspect', a.id)}
              >
                {a.label}
              </button>
            ))}
          </div>
        </div>

        <div className="row" style={{ justifyContent: 'flex-end', marginBottom: 0 }}>
          <button className="btn primary" onClick={onClose}>
            Tutup
          </button>
        </div>
      </div>
    </div>
  );
}
