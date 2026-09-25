/**
 * LUT: parser file .cube → sampler3D 32³ (RGBA8).
 * App beauty/PR komersial semuanya grading pakai LUT; ini jalur yang sama,
 * plus preset analitik untuk look bawaan (tanpa aset biner).
 */

export interface Lut {
  size: number;
  data: Uint8Array; // size³ * RGBA
  name: string;
}

export function parseCube(text: string, name = 'custom.cube'): Lut | null {
  let size = 0;
  const nums: number[] = [];
  let domainMin = 0;
  let domainMax = 1;
  for (const raw of text.split(/\r?\n/)) {
    const l = raw.trim();
    if (!l || l.startsWith('#')) continue;
    if (l.startsWith('TITLE')) continue;
    if (l.startsWith('LUT_1D_SIZE') || l.startsWith('LUT_3D_SIZE')) {
      size = parseInt(l.split(/\s+/)[1], 10);
      continue;
    }
    if (l.startsWith('DOMAIN_MIN') || l.startsWith('DOMAIN_MAX')) {
      const v = l.split(/\s+/).slice(1).map(Number);
      if (l.startsWith('DOMAIN_MIN')) domainMin = v[0] ?? 0;
      else domainMax = v[2] ?? 1;
      continue;
    }
    const p = l.split(/\s+/).map(Number);
    if (p.length >= 3 && p.every((n) => Number.isFinite(n))) nums.push(p[0], p[1], p[2]);
  }
  if (!size || size < 2) return null;
  const n = size * size * size * 3;
  const src = new Float32Array(n);
  for (let i = 0; i < Math.min(nums.length, n); i++) src[i] = nums[i];
  const data = new Uint8Array(size * size * size * 4);
  const span = Math.max(domainMax - domainMin, 1e-6);
  for (let i = 0; i < size * size * size; i++) {
    for (let c = 0; c < 3; c++) {
      let v = src[i * 3 + c];
      if (!Number.isFinite(v)) v = 0;
      v = (v - domainMin) / span;
      data[i * 4 + c] = Math.round(Math.max(0, Math.min(1, v)) * 255);
    }
    data[i * 4 + 3] = 255;
  }
  return { size, data, name };
}

/**
 * Bangun 3D LUT dari koreksi analitik (untuk preset yang butuh kurva presisi,
 * mis. "fade" + hue-rotate highlight). Dipakai kalau user tidak load .cube.
 */
export function lutFromParams(
  size: number,
  f: (r: number, g: number, b: number) => [number, number, number]
): Lut {
  const data = new Uint8Array(size * size * size * 4);
  for (let b = 0; b < size; b++) {
    for (let g = 0; g < size; g++) {
      for (let r = 0; r < size; r++) {
        const [R, G, B] = f(r / (size - 1), g / (size - 1), b / (size - 1));
        const i = (b * size * size + g * size + r) * 4;
        data[i] = Math.round(Math.max(0, Math.min(1, R)) * 255);
        data[i + 1] = Math.round(Math.max(0, Math.min(1, G)) * 255);
        data[i + 2] = Math.round(Math.max(0, Math.min(1, B)) * 255);
        data[i + 3] = 255;
      }
    }
  }
  return { size, data, name: 'procedural' };
}
