/** uji logika murni (tanpa GPU): geometri wajah, mesh warp, mask, LUT */
import { FaceMesh, buildGeom } from '../src/face/mesh';
import { MaskBuilder } from '../src/fx/masks';
import { N_POINTS, IDX } from '../src/face/landmarks';
import { DEFAULT_PARAMS } from '../src/fx/params';
import { parseCube, lutFromParams } from '../src/fx/lut';

const face = (() => {
  // wajah sintetis: 68 titik pada grid (deterministik, mirip proporsi wajah)
  const p = new Float32Array(N_POINTS * 2);
  const cx = 0.5, cy = 0.5, w = 0.09, h = 0.13;
  for (let i = 0; i < 17; i++) {
    const t = i / 16;
    const a = Math.PI * (0.05 + 0.9 * t);
    p[i * 2] = cx - Math.cos(a) * w * 2.0;
    p[i * 2 + 1] = cy + Math.sin(a) * h * 0.75 - 0.02;
  }
  const put = (i: number, x: number, y: number) => { p[i * 2] = x; p[i * 2 + 1] = y; };
  for (let i = 0; i < 5; i++) {
    put(17 + i, cx - w + (i / 4) * w * 0.9, cy - h * 0.62 - Math.sin((i / 4) * Math.PI) * h * 0.08);
    put(22 + i, cx + w - (i / 4) * w * 0.9, cy - h * 0.62 - Math.sin((i / 4) * Math.PI) * h * 0.08);
  }
  for (let i = 0; i < 4; i++) put(27 + i, cx, cy - h * (0.5 - i * 0.18));
  put(31, cx - w * 0.62); p[31 * 2 + 1] = cy + h * 0.1;
  put(32, cx - w * 0.3, cy + h * 0.14);
  put(33, cx, cy + h * 0.17);
  put(34, cx + w * 0.3, cy + h * 0.14);
  p[35 * 2] = cx + w * 0.62; p[35 * 2 + 1] = cy + h * 0.1;
  for (let i = 0; i < 6; i++) {
    const a = (i / 6) * Math.PI * 2;
    put(36 + i, cx - w * 0.85 + Math.cos(a) * w * 0.42, cy - h * 0.28 + Math.sin(a) * h * 0.14);
    put(42 + i, cx + w * 0.85 + Math.cos(a) * w * 0.42, cy - h * 0.28 + Math.sin(a) * h * 0.14);
  }
  for (let i = 0; i < 12; i++) {
    const a = (i / 12) * Math.PI * 2;
    put(48 + i, cx + Math.cos(a) * w * 0.72, cy + h * 0.52 + Math.sin(a) * h * 0.16);
  }
  for (let i = 0; i < 8; i++) {
    const a = (i / 8) * Math.PI * 2;
    put(60 + i, cx + Math.cos(a) * w * 0.45, cy + h * 0.52 + Math.sin(a) * h * 0.09);
  }
  return p;
})();

export function run() {
  const results: string[] = [];
  const assert = (name: string, cond: boolean, info = '') => {
    results.push(`${cond ? 'PASS' : 'FAIL'}  ${name}${info ? ' — ' + info : ''}`);
    if (!cond) process.exitCode = 1;
  };

  const g = buildGeom(face, 1, [0.3, 0.25, 0.4, 0.5], { happy: 0.7, surprised: 0.1, neutral: 0.2 });
  assert('geom: 68 titik valid (finite, 0..1)', Array.from(g.pts).every((v) => Number.isFinite(v) && v > -0.2 && v < 1.2));
  assert('geom: roll/yaw/pitch finite', [g.roll, g.yaw, g.pitch].every(Number.isFinite), `roll=${g.roll.toFixed(3)} yaw=${g.yaw.toFixed(3)}`);
  assert('geom: eye radius > 0', g.eyeR.rx > 0 && g.eyeL.ry > 0);
  assert('geom: width masuk akal', g.width > 0.05 && g.width < 0.6, g.width.toFixed(3));

  const mesh = new FaceMesh();
  mesh.layout(g);
  assert('mesh: anchor src = count*2 finite', mesh.base.length === mesh.count * 2 && Array.from(mesh.base).every(Number.isFinite), `${mesh.count} anchor`);
  const tris = mesh.triangulate();
  assert('mesh: delaunay menghasilkan segitiga', tris > mesh.count * 2, `${tris / 3} segitiga`);
  // semua anchor harus ter-cover & di dalam/di sekitar frame
  let minX = 1e9, maxX = -1e9;
  for (let i = 0; i < mesh.count; i++) { minX = Math.min(minX, mesh.base[i * 2]); maxX = Math.max(maxX, mesh.base[i * 2]); }
  assert('mesh: boundary menutupi lebar frame', minX < 0.02 && maxX > 0.98, `${minX.toFixed(2)}..${maxX.toFixed(2)}`);

  const p = { ...DEFAULT_PARAMS };
  mesh.displace(g, p, 16 / 9, 1);
  let moved = 0, maxD = 0, bgMax = 0;
  for (let i = 0; i < mesh.count; i++) {
    const d = Math.hypot(mesh.dest[i * 2] - mesh.base[i * 2], mesh.dest[i * 2 + 1] - mesh.base[i * 2 + 1]);
    if (d > 3e-4) moved++;
    maxD = Math.max(maxD, d);
  }
  // invariant penting: anchor DI LUAR kotak wajah tidak boleh bergeser (background stabil)
  let outside = 0;
  // "background" = di luar jejak mesh wajah (lebar 1.25×wajah, jidat s/d bawah dagu)
  const cx = (g.eyeR.c.x + g.eyeL.c.x) / 2;
  const x0 = cx - g.width * 1.25;
  const x1 = cx + g.width * 1.25;
  const y0 = Math.min(...Array.from({ length: 68 }, (_, i) => g.pts[i * 2 + 1])) - 0.16;
  const y1 = Math.max(...Array.from({ length: 68 }, (_, i) => g.pts[i * 2 + 1])) + 0.06;
  for (let i = 0; i < mesh.count; i++) {
    const bx = mesh.base[i * 2];
    const by = mesh.base[i * 2 + 1];
    if (bx < x0 || bx > x1 || by < y0 || by > y1) {
      outside++;
      bgMax = Math.max(bgMax, Math.hypot(mesh.dest[i * 2] - mesh.base[i * 2], mesh.dest[i * 2 + 1] - mesh.base[i * 2 + 1]));
    }
  }
  assert('warp: hanya area wajah yang bergeser signifikan', moved > 10 && moved < mesh.count * 0.75, `${moved}/${mesh.count}, max ${maxD.toFixed(4)}`);
  assert('warp: background di luar kotak wajah stabil', bgMax < 0.0015, `${outside} anchor luar, max geser ${bgMax.toFixed(5)}`);
  assert('warp: displacement kecil & halus', maxD < 0.05, maxD.toFixed(4));
  const zero = { ...DEFAULT_PARAMS, smooth: 0, jawSlim: 0, cheek: 0, chin: 0, forehead: 0, eyeEnlarge: 0, eyeSpacing: 0, noseSlim: 0, noseShort: 0, lipPlump: 0, smile: 0, browLift: 0, earFade: 0 };
  mesh.displace(g, zero, 16 / 9, 1);
  let movedZero = 0;
  for (let i = 0; i < mesh.count; i++) if (Math.hypot(mesh.dest[i * 2] - mesh.base[i * 2], mesh.dest[i * 2 + 1] - mesh.base[i * 2 + 1]) > 1e-7) movedZero++;
  assert('warp: identitas saat semua param 0', movedZero === 0, `${movedZero} bergeser`);

  const mb = new MaskBuilder();
  const mf = mb.build(mesh.dest, g, p, false);
  assert('mask: kulit terisi', mf.an > 40, `${mf.an} verteks`);
  assert('mask: region B & C terisi', mf.bn > 60 && mf.cn > 20, `B=${mf.bn} C=${mf.cn}`);
  const all = [mf.a, mf.b, mf.c];
  let finite = true, inRange = true, weights = 0;
  for (let ti = 0; ti < 3; ti++) {
    const arr = all[ti].subarray(0, [mf.an, mf.bn, mf.cn][ti] * 6);
    for (let i = 0; i < arr.length; i += 6) {
      if (!Number.isFinite(arr[i]) || !Number.isFinite(arr[i + 1])) finite = false;
      if (arr[i] < -0.25 || arr[i] > 1.25 || arr[i + 1] < -0.25 || arr[i + 1] > 1.25) inRange = false;
      for (let c = 2; c < 6; c++) { weights += arr[i + c]; if (arr[i + c] < -1e-6 || arr[i + c] > 1.0001) inRange = false; }
    }
  }
  assert('mask: semua koordinat finite', finite);
  assert('mask: koordinat & bobot dalam rentang', inRange, `total bobot ${weights.toFixed(0)}`);
  const empty = mb.build(mesh.dest, { ...g, vis: 0 }, p, false);
  assert('mask: vis=0 → geometri kosong', empty.an === 0 && empty.bn === 0 && empty.cn === 0);

  // LUT
  const cube = `TITLE x\nLUT_3D_SIZE 2\n0 0 0\n0 0 1\n0 1 0\n0 1 1\n1 0 0\n1 0 1\n1 1 0\n1 1 1`;
  const lut = parseCube(cube, 't.cube');
  assert('lut: parser .cube', !!lut && lut.size === 2 && lut.data.length === 8 * 4);
  const proc = lutFromParams(4, (r, gc, b) => [1 - r, gc, b]);
  assert('lut: generator prosedural', proc.data.length === 64 * 4 && proc.data[0] === 255 && proc.data[3] === 255);

  // indeks region tidak bocor
  assert('landmark: indeks region dalam rentang', [...IDX.jaw, ...IDX.eyeR, ...IDX.lipOuter, 67].every((i) => i < N_POINTS));

  console.log(results.join('\n'));
  console.log(results.some((r) => r.startsWith('FAIL')) ? `\n❌ ada kegagalan` : `\n✅ ${results.length} assertion lolos`);
}
run();
