import { useCallback, useEffect, useMemo, useRef, useState, useSyncExternalStore } from 'react';
import { store } from '../state';
import type { FxParams } from '../fx/params';

/** slice reactive dari store (engine tetap membaca objek mentah → render loop tidak terpengaruh React) */
export function useApp() {
  return useSyncExternalStore(store.subscribe, store.getSnapshot, store.getSnapshot);
}

export function useToast() {
  const [msg, setMsg] = useState<string | null>(null);
  const t = useRef(0);
  const toast = useCallback((m: string) => {
    setMsg(m);
    window.clearTimeout(t.current);
    t.current = window.setTimeout(() => setMsg(null), 2800);
  }, []);
  useEffect(() => () => window.clearTimeout(t.current), []);
  return { msg, toast };
}

export function setParam<K extends keyof FxParams>(k: K, v: FxParams[K]) {
  store.setParam(k, v);
}

/** slider beautify; klik-dua-kali untuk reset ke default */
export function Slider({
  label,
  value,
  min,
  max,
  step = 0.01,
  onChange,
  def,
}: {
  label: string;
  value: number;
  min: number;
  max: number;
  step?: number;
  onChange: (v: number) => void;
  def?: number;
}) {
  const pct = useMemo(() => ((value - min) / (max - min)) * 100, [value, min, max]);
  const shown = Math.abs(max - min) <= 2 ? value.toFixed(2) : value.toFixed(1);
  return (
    <label
      className="slider"
      title="klik dua kali = reset"
      onDoubleClick={() => def !== undefined && onChange(def)}
    >
      <span className="lab">
        <span>{label}</span>
        <b>{shown}</b>
      </span>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        style={{ ['--pct' as string]: `${Math.max(0, Math.min(100, pct))}%` }}
        onChange={(e) => onChange(parseFloat(e.target.value))}
      />
    </label>
  );
}

export function Chip({
  children,
  on,
  onClick,
  className = '',
  title,
}: {
  children: React.ReactNode;
  on?: boolean;
  onClick?: () => void;
  className?: string;
  title?: string;
}) {
  return (
    <button className={`pill ${on ? 'on' : ''} ${className}`} onClick={onClick} title={title}>
      {children}
    </button>
  );
}

export type RGB = { r: number; g: number; b: number };

export function Swatches({ value, colors, onChange }: { value: RGB; colors: RGB[]; onChange: (c: RGB) => void }) {
  return (
    <div className="swatches">
      {colors.map((c, i) => {
        const on = Math.abs(c.r - value.r) < 0.02 && Math.abs(c.g - value.g) < 0.02 && Math.abs(c.b - value.b) < 0.02;
        return (
          <button
            key={i}
            className={`sw ${on ? 'on' : ''}`}
            style={{ background: `rgb(${c.r * 255} ${c.g * 255} ${c.b * 255})` }}
            onClick={() => onChange(c)}
          />
        );
      })}
      <label className="sw" title="warna kustom" style={{ background: 'conic-gradient(red,yellow,lime,cyan,blue,magenta,red)' }}>
        <input
          type="color"
          value={rgbToHex(value)}
          onChange={(e) => onChange(hexToRgb(e.target.value))}
          style={{ opacity: 0, width: '100%', height: '100%' }}
        />
      </label>
    </div>
  );
}

export function hexToRgb(hex: string): RGB {
  const m = hex.replace('#', '');
  const v = m.length === 3 ? m.split('').map((c) => c + c).join('') : m;
  const n = parseInt(v.slice(0, 6), 16);
  return { r: ((n >> 16) & 255) / 255, g: ((n >> 8) & 255) / 255, b: (n & 255) / 255 };
}
export function rgbToHex(c: RGB) {
  const h = (v: number) => Math.round(Math.max(0, Math.min(1, v)) * 255).toString(16).padStart(2, '0');
  return `#${h(c.r)}${h(c.g)}${h(c.b)}`;
}

export function fmtMs(ms: number) {
  const s = Math.floor(ms / 1000);
  return `${String(Math.floor(s / 60)).padStart(2, '0')}:${String(s % 60).padStart(2, '0')}`;
}
