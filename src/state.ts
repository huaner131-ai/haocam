/**
 * state — store parameter kecil (external store) yang dipakai React + engine.
 * Engine membaca objek params yang SAMA tiap frame (mutasi ringan, tanpa alokasi),
 * React subscribe untuk re-render. Preset disimpan ke localStorage.
 */
import { DEFAULT_PARAMS, type FxParams, type Quality, type Aspect, ASPECTS } from './fx/params';

export interface AppState {
  params: FxParams;
  /** master intensity untuk seluruh beautify (0 = raw, 1 = penuh) */
  masterBeauty: number;
  beautyId: string;
  filterId: string;
  quality: Quality;
  accuracy: 'fast' | 'sharp';
  showMesh: boolean;
  compare: number; // 0 = off, 0..1 posisi split
  recordFps: number;
  bitrateMbps: number;
  audio: boolean;
  mirror: boolean;
  running: boolean;
}

type Listener = () => void;
const LS_KEY = 'haocam.v1.params';

export class Store {
  state: AppState = {
    params: { ...DEFAULT_PARAMS },
    masterBeauty: 1,
    beautyId: 'auto',
    filterId: 'orig',
    quality: 'balanced',
    accuracy: 'fast',
    showMesh: false,
    compare: 0,
    recordFps: 30,
    bitrateMbps: 8,
    audio: true,
    mirror: true,
    running: false,
  };
  private listeners = new Set<Listener>();
  private snapshot = { ...this.state };

  constructor() {
    this.load();
  }

  subscribe = (fn: Listener) => {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  };
  getSnapshot = () => this.snapshot;

  private emit() {
    this.snapshot = { ...this.state, params: { ...this.state.params } };
    for (const l of this.listeners) l();
  }

  set<K extends keyof AppState>(key: K, value: AppState[K]) {
    this.state[key] = value;
    if (key !== 'params') this.emit();
  }

  setParam<K extends keyof FxParams>(key: K, value: FxParams[K]) {
    this.state.params[key] = value;
    if (key === 'aspect') {
      const a = ASPECTS.find((x) => x.id === (value as Aspect)) ?? ASPECTS[0];
      this.state.params.aspectRatio = a.ratio;
    }
    if (key === 'mirror') this.state.mirror = !!value;
    this.emit();
    this.saveDebounced();
  }

  patch(p: Partial<FxParams>, reset = false) {
    if (reset) this.state.params = { ...DEFAULT_PARAMS, ...p };
    else Object.assign(this.state.params, p);
    this.emit();
    this.saveDebounced();
  }

  reset() {
    this.state.params = { ...DEFAULT_PARAMS };
    this.emit();
    this.saveDebounced();
  }

  setMaster(v: number) {
    this.state.masterBeauty = Math.max(0, Math.min(1, v));
    this.emit();
  }
  markPreset(beautyId: string, filterId?: string) {
    this.state.beautyId = beautyId;
    if (filterId) this.state.filterId = filterId;
    this.emit();
  }

  /** ref langsung untuk render loop (tanpa copy) */
  get live() {
    return this.state;
  }

  /* ── custom presets ──────────────────────────────────────── */
  private savedKey = 'haocam.v1.presets';
  listPresets(): { name: string; params: FxParams }[] {
    try {
      return JSON.parse(localStorage.getItem(this.savedKey) ?? '[]');
    } catch {
      return [];
    }
  }
  savePreset(name: string) {
    const list = this.listPresets().filter((x) => x.name !== name);
    list.unshift({ name, params: this.state.params });
    try {
      localStorage.setItem(this.savedKey, JSON.stringify(list.slice(0, 24)));
    } catch {
      /* quota */
    }
    this.emit();
  }
  deletePreset(name: string) {
    const list = this.listPresets().filter((x) => x.name !== name);
    try {
      localStorage.setItem(this.savedKey, JSON.stringify(list));
    } catch {
      /* noop */
    }
    this.emit();
  }
  loadPreset(params: FxParams) {
    this.state.params = { ...DEFAULT_PARAMS, ...params };
    this.emit();
  }

  private saveDebounced() {
    clearTimeout(this.t);
    this.t = window.setTimeout(() => {
      try {
        localStorage.setItem(LS_KEY, JSON.stringify(this.state.params));
      } catch {
        /* noop */
      }
    }, 400);
  }
  private t = 0;
  private load() {
    try {
      const raw = localStorage.getItem(LS_KEY);
      if (raw) {
        const obj = JSON.parse(raw) as Partial<FxParams>;
        this.state.params = { ...DEFAULT_PARAMS, ...obj };
      }
    } catch {
      /* noop */
    }
    const a = ASPECTS.find((x) => x.id === this.state.params.aspect);
    this.state.params.aspectRatio = a ? a.ratio : 9 / 16;
  }
}

export const store = new Store();
