import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// HaoCam — Vite + React (app shell) + custom WebGL2 engine (fx pipeline).
// `base: './'` supaya build bisa di-serve dari sub-path apa pun.
export default defineConfig({
  base: './',
  plugins: [react()],
  server: { host: '0.0.0.0', port: 5173, strictPort: true, allowedHosts: true },
  preview: { host: '0.0.0.0', port: 4173, allowedHosts: true },
  build: { target: 'esnext', assetsInlineLimit: 0 },
});
