/**
 * Tipe untuk face-api.js (vendored di public/vendor/face-api.js, dimuat sebagai
 * UMD global `window.faceapi`). Kita hanya mendeklarasikan subset yang dipakai,
 * supaya tidak bergantung pada .d.ts besar dari paketnya.
 */
export {};

declare global {
  interface FaceBox {
    x: number;
    y: number;
    width: number;
    height: number;
  }
  interface FacePoint {
    x: number;
    y: number;
  }

  interface FaceApiResult {
    box: FaceBox;
    score: number;
    landmarks: {
      positions: FacePoint[]; // 68 titik
      /** indexed helpers yang kita pakai */
      jaw: { objectToRect?: unknown };
    };
    expressions?: Record<string, number>;
  }

  interface FaceApi {
    tf: {
      ready(): Promise<void>;
      setBackend(name: string): boolean | Promise<boolean>;
      setProdMode(flag: boolean): void;
    };
    env: { setGpuMode(gpu: unknown): void; setEnv(overrides: Record<string, unknown>): void };
    nets: {
      tinyFaceDetector: { loadFromUri(uri: string): Promise<unknown> };
      faceLandmark68Net: { loadFromUri(uri: string): Promise<unknown> };
      faceLandmark68TinyNet: { loadFromUri(uri: string): Promise<unknown> };
      faceExpressionNet: { loadFromUri(uri: string): Promise<unknown> };
    };
    TinyFaceDetectorOptions: new (opts: {
      inputSize?: number;
      scoreThreshold?: number;
    }) => unknown;
    SsdMobilenetv1Options: new (opts: { minConfidence?: number; maxResults?: number }) => unknown;
    detectAllFaces(
      input: unknown,
      options: unknown
    ): {
      withFaceLandmarks(useTiny: boolean): {
        withFaceExpressions(): Promise<FaceApiResult[]>;
      };
    };
    matchDimensions?: unknown;
  }

  interface Window {
    faceapi?: FaceApi;
  }
}
