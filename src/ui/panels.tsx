import { useState } from 'react';
import {
  BEAUTY_PRESETS,
  DEFAULT_PARAMS,
  FILTERS,
  FRAMES,
  LENSES,
  TAB_SLIDERS,
  type SliderDef,
} from '../fx/params';
import { store, type AppState } from '../state';
import { Chip, Slider, Swatches, type RGB } from './controls';

/* ── aksi preset ─────────────────────────────────────────── */
export function applyBeauty(id: string) {
  const p = BEAUTY_PRESETS.find((x) => x.id === id);
  if (!p) return;
  store.patch(p.patch);
  store.markPreset(id);
}
export function applyFilter(id: string) {
  const f = FILTERS.find((x) => x.id === id) ?? FILTERS[0];
  const neutral: Record<string, number> = {};
  for (const k of Object.keys(DEFAULT_PARAMS)) {
    const key = k as keyof typeof DEFAULT_PARAMS;
    if (typeof DEFAULT_PARAMS[key] === 'number' && (DEFAULT_PARAMS.aspect as string) !== '' && isGrade(key)) neutral[key] = DEFAULT_PARAMS[key] as number;
  }
  store.patch({ ...neutral, ...f.patch });
  store.markPreset(store.getSnapshot().beautyId, id);
}
function isGrade(k: string) {
  return [
    'exposure', 'contrast', 'saturation', 'temp', 'tint2', 'highlights', 'shadows', 'fade', 'grain', 'vignette',
    'bloom', 'halation', 'softFocus', 'sharpness', 'clarity', 'ringLight', 'lutStrength',
  ].includes(k);
}

function Group({ st, defs }: { st: AppState; defs: SliderDef[] }) {
  const p = st.params;
  return (
    <div className="grid-cards" style={{ gridTemplateColumns: 'repeat(auto-fit, minmax(220px,1fr))', gap: '0 18px' }}>
      {defs.map((d) => (
        <Slider
          key={d.key as string}
          label={d.label}
          min={d.min}
          max={d.max}
          step={d.step ?? 0.01}
          value={p[d.key] as number}
          def={DEFAULT_PARAMS[d.key] as number}
          onChange={(v) => store.setParam(d.key, v as never)}
        />
      ))}
    </div>
  );
}

/* ── BEAUTY ──────────────────────────────────────────────── */
export function BeautyPanel({ st }: { st: AppState }) {
  const [tab, setTab] = useState<'skin' | 'reshape' | 'eyes' | 'makeup'>('skin');
  const p = st.params;
  return (
    <>
      <div className="grid-cards" style={{ gridTemplateColumns: 'repeat(auto-fill,minmax(110px,1fr))' }}>
        {BEAUTY_PRESETS.map((b) => (
          <button key={b.id} className={`card ${st.beautyId === b.id ? 'on' : ''}`} onClick={() => applyBeauty(b.id)}>
            <span className="n">{b.name}</span>
            <span className="h">{b.hint}</span>
          </button>
        ))}
      </div>
      <div className="row" style={{ marginTop: 10 }}>
        <span className="note" style={{ flex: 1 }}>
          Master beautify
        </span>
        <div style={{ width: 150 }}>
          <Slider label="" value={st.masterBeauty} min={0} max={1} onChange={(v) => store.setMaster(v)} />
        </div>
      </div>
      <div className="row">
        {(['skin', 'reshape', 'eyes', 'makeup'] as const).map((t) => (
          <Chip key={t} on={tab === t} onClick={() => setTab(t)}>
            {t === 'skin' ? 'Kulit' : t === 'reshape' ? 'Bentuk Wajah' : t === 'eyes' ? 'Mata & Gigi' : 'Makeup'}
          </Chip>
        ))}
        {tab === 'makeup' && (
          <Chip on={p.makeupOn > 0.5} onClick={() => store.setParam('makeupOn', p.makeupOn > 0.5 ? 0 : 1)}>
            Makeup {p.makeupOn > 0.5 ? 'on' : 'off'}
          </Chip>
        )}
      </div>
      <Group st={st} defs={TAB_SLIDERS[tab]} />
      {tab === 'makeup' && (
        <>
          <div className="group-title">warna</div>
          <div className="row" style={{ alignItems: 'center', gap: 14 }}>
            <div style={{ display: 'grid', gap: 3 }}>
              <span className="note">Blush</span>
              <Swatches value={p.blushColor} onChange={(c: RGB) => store.setParam('blushColor', c)} colors={PALETTE_BLUSH} />
            </div>
            <div style={{ display: 'grid', gap: 3 }}>
              <span className="note">Bibir</span>
              <Swatches value={p.lipColor} onChange={(c: RGB) => store.setParam('lipColor', c)} colors={PALETTE_LIP} />
            </div>
            <div style={{ display: 'grid', gap: 3 }}>
              <span className="note">Kelopak</span>
              <Swatches value={p.shadowColor} onChange={(c: RGB) => store.setParam('shadowColor', c)} colors={PALETTE_SHADOW} />
            </div>
            <div style={{ display: 'grid', gap: 3 }}>
              <span className="note">Alis</span>
              <Swatches value={p.browColor} onChange={(c: RGB) => store.setParam('browColor', c)} colors={PALETTE_BROW} />
            </div>
          </div>
        </>
      )}
      {tab === 'reshape' && (
        <p className="note">
          Bentuk wajah dipakai lewat <b>mesh warp</b>: Delaunay triangulation di atas 68 landmark, lalu GPU
          inverse-map. Kalau wajah menoleh &gt; ±45° displacement otomatis dikurangi supaya tidak melenceng.
        </p>
      )}
    </>
  );
}

/* ── FILTER ──────────────────────────────────────────────── */
export function FilterPanel({ st }: { st: AppState }) {
  return (
    <>
      <div className="grid-cards">
        {FILTERS.map((f) => (
          <button key={f.id} className={`card ${st.filterId === f.id ? 'on' : ''}`} onClick={() => applyFilter(f.id)}>
            <span className="e">{f.emoji}</span>
            <span className="n">{f.name}</span>
          </button>
        ))}
      </div>
      <div style={{ marginTop: 8 }}>
        <Group st={st} defs={TAB_SLIDERS.light} />
      </div>
    </>
  );
}

/* ── LENS AR ─────────────────────────────────────────────── */
export function LensPanel({ st }: { st: AppState }) {
  const p = st.params;
  return (
    <>
      <div className="grid-cards">
        <button className={`card ${p.lens < 0 ? 'on' : ''}`} onClick={() => store.setParam('lens', -1)}>
          <span className="e">🚫</span>
          <span className="n">Tanpa lens</span>
        </button>
        {LENSES.filter((l) => l.id !== 'none').map((l, i) => (
          <button key={l.id} className={`card ${p.lens === i + 1 ? 'on' : ''}`} onClick={() => store.setParam('lens', i + 1)}>
            <span className="e">{l.emoji}</span>
            <span className="n">{l.name}</span>
          </button>
        ))}
      </div>
      <div style={{ marginTop: 6 }}>
        <Slider label="Intensitas lens" min={0} max={1} value={p.lensIntensity} def={1} onChange={(v) => store.setParam('lensIntensity', v)} />
        <Slider label="Reaksi ekspresi (senyum / buka mulut)" min={0} max={1} value={p.autoTrigger} def={0.5} onChange={(v) => store.setParam('autoTrigger', v)} />
      </div>
      <p className="note">
        Lens digambar prosedural di Canvas2D lalu di-anchor ke face mesh (roll, skala wajah, garis rambut) — jadi
        dia ikut menoleh & menunduk, tanpa file aset.
      </p>
    </>
  );
}

/* ── FRAME ───────────────────────────────────────────────── */
export function FramePanel({ st }: { st: AppState }) {
  const p = st.params;
  return (
    <>
      <div className="grid-cards">
        {FRAMES.map((f, i) => (
          <button key={f.id} className={`card ${p.frame === i ? 'on' : ''}`} onClick={() => store.setParam('frame', i)}>
            <span className="e">{f.emoji}</span>
            <span className="n">{f.name}</span>
          </button>
        ))}
      </div>
      <div style={{ marginTop: 6 }}>
        <Slider label="Opasitas frame" min={0} max={1} value={p.frameAlpha} def={1} onChange={(v) => store.setParam('frameAlpha', v)} />
      </div>
    </>
  );
}

/* ── PRO ─────────────────────────────────────────────────── */
export function ProPanel({ st }: { st: AppState }) {
  const [name, setName] = useState('look-1');
  const saved = store.listPresets();
  return (
    <>
      <div className="group-title">kulit</div>
      <Group st={st} defs={TAB_SLIDERS.skin} />
      <div className="group-title">bentuk wajah</div>
      <Group st={st} defs={TAB_SLIDERS.reshape} />
      <div className="group-title">mata &amp; gigi</div>
      <Group st={st} defs={TAB_SLIDERS.eyes} />
      <div className="group-title">makeup</div>
      <Group st={st} defs={TAB_SLIDERS.makeup} />
      <div className="group-title">light &amp; grade</div>
      <Group st={st} defs={TAB_SLIDERS.light} />
      <div className="group-title">look tersimpan</div>
      <div className="row">
        <input
          className="pill"
          style={{ background: 'rgba(255,255,255,.06)' }}
          value={name}
          onChange={(e) => setName(e.target.value)}
          placeholder="nama look"
        />
        <Chip onClick={() => store.savePreset(name.trim() || 'look')}>Simpan</Chip>
        <Chip
          onClick={() => {
            const a = document.createElement('a');
            a.href = URL.createObjectURL(new Blob([JSON.stringify(st.params, null, 2)], { type: 'application/json' }));
            a.download = 'haocam-look.json';
            a.click();
          }}
        >
          Export JSON
        </Chip>
        <Chip
          onClick={() => {
            const inp = document.createElement('input');
            inp.type = 'file';
            inp.accept = '.json,application/json';
            inp.onchange = async () => {
              const f = inp.files?.[0];
              if (!f) return;
              try {
                store.loadPreset(JSON.parse(await f.text()));
              } catch {
                /* ignore */
              }
            };
            inp.click();
          }}
        >
          Import JSON
        </Chip>
        <Chip onClick={() => store.reset()}>Reset</Chip>
      </div>
      <div className="list">
        {saved.map((x) => (
          <button key={x.name} onClick={() => store.loadPreset(x.params)}>
            <span>{x.name}</span>
            <span
              style={{ color: '#8b8ba3' }}
              onClick={(e) => {
                e.stopPropagation();
                store.deletePreset(x.name);
              }}
            >
              hapus
            </span>
          </button>
        ))}
        {!saved.length && <span className="note">Belum ada look tersimpan (localStorage).</span>}
      </div>
    </>
  );
}

const PALETTE_BLUSH: RGB[] = [
  { r: 0.94, g: 0.45, b: 0.5 },
  { r: 0.98, g: 0.6, b: 0.52 },
  { r: 0.86, g: 0.34, b: 0.42 },
  { r: 0.96, g: 0.52, b: 0.66 },
  { r: 0.79, g: 0.36, b: 0.36 },
];
const PALETTE_LIP: RGB[] = [
  { r: 0.82, g: 0.18, b: 0.24 },
  { r: 0.93, g: 0.35, b: 0.42 },
  { r: 0.68, g: 0.13, b: 0.25 },
  { r: 0.96, g: 0.47, b: 0.44 },
  { r: 0.55, g: 0.11, b: 0.17 },
  { r: 0.88, g: 0.55, b: 0.5 },
];
const PALETTE_SHADOW: RGB[] = [
  { r: 0.66, g: 0.42, b: 0.44 },
  { r: 0.52, g: 0.36, b: 0.55 },
  { r: 0.8, g: 0.6, b: 0.46 },
  { r: 0.35, g: 0.3, b: 0.4 },
  { r: 0.9, g: 0.72, b: 0.66 },
];
const PALETTE_BROW: RGB[] = [
  { r: 0.28, g: 0.19, b: 0.16 },
  { r: 0.2, g: 0.14, b: 0.12 },
  { r: 0.38, g: 0.26, b: 0.19 },
  { r: 0.46, g: 0.34, b: 0.27 },
];
