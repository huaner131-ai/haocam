/*
 * Facebetter C API (SDK 2.0)
 *
 * Copyright © 2025 facebetter. All rights reserved.
 *
 * The shared C ABI for Flutter / Unity / Web (WASM) / native FFI.
 *
 * Design notes
 * ------------
 * 1. All symbols use the `fb_` prefix to avoid clashes with other static libs.
 * 2. Prefer native textures (fb_process_texture) or session slots for realtime
 *    video so callers do not own pixel memory. Session pixel buffers are
 *    allocated and recycled by the slot pool.
 * 3. Parameter setters (fb_set_*) are synchronous and safe to call from the UI
 *    thread. APIs with an `_async` suffix run on an internal worker and report
 *    completion via callback. WASM is single-threaded: `_async` runs on the
 *    calling thread and then fires the callback.
 * 4. Video frames use an acquire -> write -> submit -> callback -> release
 *    slot cycle. Input and output slots are paired by the same index.
 * 5. Style / colour enum values are passed as int32_t. Definitions live in
 *    src/include/facebetter/beauty_params.h.
 *
 * Threading
 * ---------
 * - fb_engine_* / fb_set_* : any thread; internally locked.
 * - fb_session_acquire / submit / release : lock-free, non-blocking; OK on UI.
 * - Callbacks (fb_frame_cb / fb_task_cb): native platforms fire on the SDK
 *   worker thread; WASM fires synchronously on the calling thread. On Dart,
 *   use NativeCallable.listener to receive them.
 */

#ifndef FACEBETTER_ENGINE_C_FB_C_API_H_
#define FACEBETTER_ENGINE_C_FB_C_API_H_

#include <stdint.h>

#if defined(_WIN32)
#if defined(BUILDING_FB_DLL)
#define FB_CAPI __declspec(dllexport)
#else
#define FB_CAPI __declspec(dllimport)
#endif
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#define FB_CAPI EMSCRIPTEN_KEEPALIVE
#else
#define FB_CAPI __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Constants and status codes                                         */
/* ------------------------------------------------------------------ */

/** Maximum number of faces returned per frame. */
#define FB_MAX_FACES 5

/**
 * Floats per face in the shared landmark buffer.
 * Layout: rect(4) + face_id(1) + score(1) + pitch(1) + roll(1) + yaw(1)
 *       + key_points(111*2=222) + visibility(111) = 342
 */
#define FB_FLOATS_PER_FACE 342

/** Number of face landmarks. */
#define FB_LANDMARK_COUNT 111

/** Maximum plane count for one frame (I420 uses 3). */
#define FB_MAX_PLANES 3

/** Status codes returned by every int32_t API. */
typedef enum {
  FB_OK = 0,
  FB_ERR_INVALID_ARG = -1,     /* Invalid argument (null, size <= 0, etc.) */
  FB_ERR_NOT_INITIALIZED = -2, /* Engine not created or already destroyed */
  FB_ERR_LICENSE = -3,         /* License invalid */
  FB_ERR_UNSUPPORTED = -4,     /* Unsupported on this platform / format */
  FB_ERR_IO = -5,              /* File I/O failure */
  FB_ERR_NO_SLOT = -6,         /* Slot pool full (backpressure); drop the frame */
  FB_ERR_PROCESS = -7,         /* Internal processing failure */
  FB_ERR_OOM = -8,             /* Out of memory */
} fb_status_t;

/* ------------------------------------------------------------------ */
/* Enums                                                              */
/* ------------------------------------------------------------------ */

/** Pixel formats. Values match facebetter::Format and must not change. */
typedef enum {
  FB_PIXEL_I420 = 0,
  FB_PIXEL_NV12 = 1,
  FB_PIXEL_NV21 = 2,
  FB_PIXEL_BGRA = 3,
  FB_PIXEL_RGBA = 4,
  FB_PIXEL_BGR = 5,
  FB_PIXEL_RGB = 6,
} fb_pixel_format_t;

/** Clockwise rotation applied before processing. */
typedef enum {
  FB_ROTATION_0 = 0,
  FB_ROTATION_90 = 1,
  FB_ROTATION_180 = 2,
  FB_ROTATION_270 = 3,
} fb_rotation_t;

/** Mirror mode applied before processing, after rotation. */
typedef enum {
  FB_MIRROR_NONE = 0,
  FB_MIRROR_HORIZONTAL = 1,
  FB_MIRROR_VERTICAL = 2,
  FB_MIRROR_BOTH = 3,
} fb_mirror_t;

/** Frame type. Affects temporal smoothing and face tracking. */
typedef enum {
  FB_FRAME_IMAGE = 0, /* Still image; no inter-frame state */
  FB_FRAME_VIDEO = 1, /* Continuous video stream */
} fb_frame_type_t;

/** Log levels. Match facebetter::LogLevel. */
typedef enum {
  FB_LOG_TRACE = 0,
  FB_LOG_DEBUG = 1,
  FB_LOG_INFO = 2,
  FB_LOG_WARN = 3,
  FB_LOG_ERROR = 4,
  FB_LOG_CRITICAL = 5,
} fb_log_level_t;

/** Image encoding for fb_process_encoded_async output. */
typedef enum {
  FB_ENCODING_JPEG = 0,
  FB_ENCODING_PNG = 1,
} fb_encoding_t;

/* ------------------------------------------------------------------ */
/* Handles                                                            */
/* ------------------------------------------------------------------ */

typedef struct fb_engine fb_engine_t;
typedef struct fb_session fb_session_t;
typedef struct fb_image_result fb_image_result_t;

/* ------------------------------------------------------------------ */
/* Structs                                                            */
/* ------------------------------------------------------------------ */

/** Engine create config. Strings are owned by the caller; fb_engine_create copies them. */
typedef struct {
  const char* app_id;        /* Native online auth (with app_key). Do not pass on Web. */
  const char* app_key;       /* Native online auth. Do not pass on Web (would leak). */
  const char* license_token; /* Compact JWS, HTTP {token} envelope, or offline .lic */

  const char* resource_path; /* Directory containing resource.fbd; must not be empty */
  const char* platform_identifier; /* Bundle ID / package name; may be NULL */
  int32_t external_context;        /* 1 = reuse the calling thread's GL context */
  /* 1 = enable landmark fill-back. When on, the engine runs face detection on
   * every frame even if only skin-smoothing / LUT filters are used, so leave
   * this 0 unless you need landmarks. Engine-wide (not per-session) because
   * SetCallbacks is engine-scoped and toggling at runtime races the worker. */
  int32_t enable_landmarks;
} fb_engine_config_t;

/** Log configuration. */
typedef struct {
  int32_t console_enabled;
  int32_t file_enabled;
  int32_t level; /* fb_log_level_t */
  const char* file_name;
} fb_log_config_t;

/** Engine performance statistics. */
typedef struct {
  double fps;
  double avg_process_time_ms;
  double session_time_s;
} fb_stats_t;

/** Video session configuration. */
typedef struct {
  int32_t width;
  int32_t height;
  int32_t input_format;  /* fb_pixel_format_t */
  int32_t output_format; /* fb_pixel_format_t */
  /* Slot count. 0 means the default (3). Input and output pools are the same
   * size and paired by index. More slots absorb jitter but raise latency and
   * memory; 2–3 is typical for realtime. */
  int32_t slot_count;
} fb_session_config_t;

/**
 * Output-frame metadata header. One per output slot; written by the SDK before
 * the frame callback. Map this as a struct view — no per-frame FFI encode/decode.
 *
 * When fb_session_submit uses 90° / 270° rotation, output width/height (and
 * strides) are swapped relative to the input. The values here are authoritative;
 * strides in fb_slot_info_t are only the unrotated allocation sizes.
 */
typedef struct {
  int64_t timestamp_us; /* Echo of the timestamp passed to fb_session_submit */
  int64_t user_tag;     /* Echo of the custom tag passed to fb_session_submit */
  int32_t status;       /* fb_status_t; non-zero means this frame failed */
  int32_t width;
  int32_t height;
  int32_t format;      /* fb_pixel_format_t */
  int32_t face_count;  /* Faces detected this frame, 0..FB_MAX_FACES */
  int32_t plane_count; /* Valid plane count */
  int32_t stride[FB_MAX_PLANES];
  int32_t size[FB_MAX_PLANES];
  int32_t reserved[4];
} fb_frame_header_t;

/**
 * Slot memory layout. Query once after session create; base pointers stay
 * stable until the session is destroyed. Cache them at init — do not re-query
 * every frame.
 */
typedef struct {
  int32_t slot;        /* Slot index */
  int32_t plane_count; /* Valid plane count */
  uint8_t* plane[FB_MAX_PLANES];
  int32_t stride[FB_MAX_PLANES];
  int32_t size[FB_MAX_PLANES]; /* Allocated capacity per plane (max orientation) */
  /* Output slots only: metadata header and landmark shared memory.
   * Input slots leave these NULL / 0. */
  fb_frame_header_t* header;
  float* landmarks;       /* FB_MAX_FACES * FB_FLOATS_PER_FACE floats */
  int32_t landmarks_size; /* Byte size */
} fb_slot_info_t;

/* ------------------------------------------------------------------ */
/* Callbacks                                                          */
/* ------------------------------------------------------------------ */

/**
 * Video frame completion callback.
 *
 * @param user_data Opaque pointer from fb_session_set_frame_callback
 * @param slot      Same index returned by fb_session_acquire. Read the output
 *                  header / planes for that slot, then call fb_session_release
 *                  when done. Failing to release exhausts the pool and makes
 *                  later acquires return FB_ERR_NO_SLOT.
 *
 * Fired on the SDK worker thread (native). With Dart NativeCallable.listener,
 * delivery is posted to the isolate event loop; the slot remains valid until
 * you release it.
 */
typedef void (*fb_frame_cb)(void* user_data, int32_t slot);

/**
 * Engine event callback (license result, init complete, etc.).
 * `message` is valid only for the duration of the callback; copy if needed.
 */
typedef void (*fb_event_cb)(void* user_data, int32_t code, const char* message);

/**
 * Async task completion (file ops and other jobs with no payload).
 * @param status fb_status_t
 */
typedef void (*fb_task_cb)(void* user_data, int64_t task_id, int32_t status);

/**
 * Async image-processing completion.
 * @param result Non-NULL on success — call fb_image_result_release when done.
 *               NULL on failure; inspect status for the reason.
 */
typedef void (*fb_image_cb)(void* user_data,
                            int64_t task_id,
                            fb_image_result_t* result,
                            int32_t status);

/* ------------------------------------------------------------------ */
/* Global                                                             */
/* ------------------------------------------------------------------ */

/** SDK version string (static storage; do not free). */
FB_CAPI const char* fb_version(void);

/** Set global log configuration. May be called before creating an engine. */
FB_CAPI int32_t fb_set_log_config(const fb_log_config_t* config);

/* ------------------------------------------------------------------ */
/* Engine lifecycle                                                   */
/* ------------------------------------------------------------------ */

/**
 * Create an engine.
 * Native: provide license_token, or both app_id and app_key.
 * Web: provide license_token (server-issued v2 token / response). The engine
 * does not fetch tokens on Web.
 * @return Handle on success, NULL on failure.
 */
FB_CAPI fb_engine_t* fb_engine_create(const fb_engine_config_t* config);

/**
 * Destroy an engine. Stops and destroys any open sessions and joins the worker.
 * All handles derived from this engine become invalid immediately.
 */
FB_CAPI void fb_engine_destroy(fb_engine_t* engine);

/** Register an engine event callback. Pass NULL to clear. */
FB_CAPI int32_t fb_engine_set_event_callback(fb_engine_t* engine,
                                             fb_event_cb callback,
                                             void* user_data);

/** Read performance statistics. */
FB_CAPI int32_t fb_engine_get_stats(fb_engine_t* engine, fb_stats_t* out_stats);

/* ------------------------------------------------------------------ */
/* Basic beauty (synchronous, cheap)                                  */
/* ------------------------------------------------------------------ */

FB_CAPI int32_t fb_set_smoothing(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_smoothing_style(fb_engine_t* engine, int32_t style);
FB_CAPI int32_t fb_set_whitening(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_whitening_style(fb_engine_t* engine, int32_t style);
FB_CAPI int32_t fb_set_sharpening(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_rosiness(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_beauty_skin_only(fb_engine_t* engine, int32_t enabled);

/* ------------------------------------------------------------------ */
/* Face reshape                                                       */
/* ------------------------------------------------------------------ */

/** @param param beauty_params::Reshape; intensity in [-1.0, 1.0] */
FB_CAPI int32_t fb_set_reshape(fb_engine_t* engine,
                               int32_t param,
                               float intensity);

/* ------------------------------------------------------------------ */
/* Body reshape                                                       */
/* ------------------------------------------------------------------ */

/** @param param beauty_params::BodyReshape; intensity in [0.0, 1.0] */
FB_CAPI int32_t fb_set_body_reshape(fb_engine_t* engine,
                                    int32_t param,
                                    float intensity);

/* ------------------------------------------------------------------ */
/* Makeup                                                             */
/* ------------------------------------------------------------------ */

FB_CAPI int32_t fb_set_lipstick(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_lipstick_color(fb_engine_t* engine, int32_t color);
FB_CAPI int32_t fb_set_blush(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_blush_style(fb_engine_t* engine, int32_t style);
FB_CAPI int32_t fb_set_blush_color(fb_engine_t* engine, int32_t color);
FB_CAPI int32_t fb_set_contour(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_contour_style(fb_engine_t* engine, int32_t style);
FB_CAPI int32_t fb_set_eye_shadow(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_eye_shadow_style(fb_engine_t* engine, int32_t style);
FB_CAPI int32_t fb_set_eye_shadow_color(fb_engine_t* engine, int32_t color);
FB_CAPI int32_t fb_set_eye_liner(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_eye_liner_style(fb_engine_t* engine, int32_t style);
FB_CAPI int32_t fb_set_eye_liner_color(fb_engine_t* engine, int32_t color);
FB_CAPI int32_t fb_set_eyebrow(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_eyebrow_style(fb_engine_t* engine, int32_t style);
FB_CAPI int32_t fb_set_eyebrow_color(fb_engine_t* engine, int32_t color);
FB_CAPI int32_t fb_set_eyelash(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_eyelash_style(fb_engine_t* engine, int32_t style);
FB_CAPI int32_t fb_set_eyelash_color(fb_engine_t* engine, int32_t color);
FB_CAPI int32_t fb_set_pupil(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_set_pupil_color(fb_engine_t* engine, int32_t color);

/* ------------------------------------------------------------------ */
/* Chroma key and virtual background                                  */
/* ------------------------------------------------------------------ */

FB_CAPI int32_t fb_set_chroma_key(fb_engine_t* engine, int32_t color);
FB_CAPI int32_t fb_clear_chroma_key(fb_engine_t* engine);
FB_CAPI int32_t fb_set_chroma_key_similarity(fb_engine_t* engine, float value);
FB_CAPI int32_t fb_set_chroma_key_smoothness(fb_engine_t* engine, float value);
FB_CAPI int32_t fb_set_chroma_key_desaturation(fb_engine_t* engine, float value);

FB_CAPI int32_t fb_set_virtual_background_blur(fb_engine_t* engine, float level);
FB_CAPI int32_t fb_clear_virtual_background(fb_engine_t* engine);

/* ------------------------------------------------------------------ */
/* Filter / sticker / virtual-background image (async I/O + GPU)      */
/* ------------------------------------------------------------------ */

/**
 * These APIs read files, unpack .fbd packages, and create GPU textures (tens of
 * ms). They always run asynchronously and report via fb_task_cb.
 * @return >= 0 task_id, or a negative fb_status_t on error.
 */
FB_CAPI int64_t fb_set_filter_async(fb_engine_t* engine,
                                    const char* fbd_file_path,
                                    fb_task_cb callback,
                                    void* user_data);

FB_CAPI int64_t fb_set_filter_data_async(fb_engine_t* engine,
                                         const uint8_t* fbd_data,
                                         int32_t data_size,
                                         fb_task_cb callback,
                                         void* user_data);

FB_CAPI int64_t fb_set_sticker_async(fb_engine_t* engine,
                                     const char* fbd_file_path,
                                     fb_task_cb callback,
                                     void* user_data);

FB_CAPI int64_t fb_set_sticker_data_async(fb_engine_t* engine,
                                          const uint8_t* fbd_data,
                                          int32_t data_size,
                                          fb_task_cb callback,
                                          void* user_data);

/** Extra capability pack (e.g. resource_3d.fbd). Call before Set3DSticker. */
FB_CAPI int64_t fb_add_resource_pack_async(fb_engine_t* engine,
                                           const char* fbd_file_path,
                                           fb_task_cb callback,
                                           void* user_data);

FB_CAPI int64_t fb_add_resource_pack_data_async(fb_engine_t* engine,
                                                const uint8_t* fbd_data,
                                                int32_t data_size,
                                                fb_task_cb callback,
                                                void* user_data);

/** `resource` is a path to a 3D sticker .fbd pack (sticker3d.json + mesh.obj). */
FB_CAPI int64_t fb_set_3d_sticker_async(fb_engine_t* engine,
                                        const char* resource,
                                        fb_task_cb callback,
                                        void* user_data);

FB_CAPI int64_t fb_set_3d_sticker_data_async(fb_engine_t* engine,
                                             const uint8_t* fbd_data,
                                             int32_t data_size,
                                             fb_task_cb callback,
                                             void* user_data);

FB_CAPI int64_t fb_set_virtual_background_async(fb_engine_t* engine,
                                                const char* image_path,
                                                fb_task_cb callback,
                                                void* user_data);

FB_CAPI int64_t fb_set_virtual_background_data_async(fb_engine_t* engine,
                                                     const uint8_t* image_data,
                                                     int32_t data_size,
                                                     fb_task_cb callback,
                                                     void* user_data);

/** Clearing filter / sticker is a pure state change and is synchronous. */
FB_CAPI int32_t fb_clear_filter(fb_engine_t* engine);
FB_CAPI int32_t fb_set_filter_intensity(fb_engine_t* engine, float intensity);
FB_CAPI int32_t fb_clear_sticker(fb_engine_t* engine);
FB_CAPI int32_t fb_clear_3d_sticker(fb_engine_t* engine);

/**
 * Synchronous variants. Suitable for WASM or callers already on a worker.
 * Prefer the `_async` APIs from a native UI thread to avoid blocking on I/O.
 */
FB_CAPI int32_t fb_set_filter(fb_engine_t* engine, const char* fbd_file_path);
FB_CAPI int32_t fb_set_filter_data(fb_engine_t* engine,
                                   const uint8_t* fbd_data,
                                   int32_t data_size);
FB_CAPI int32_t fb_set_sticker(fb_engine_t* engine, const char* fbd_file_path);
FB_CAPI int32_t fb_set_sticker_data(fb_engine_t* engine,
                                    const uint8_t* fbd_data,
                                    int32_t data_size);
FB_CAPI int32_t fb_add_resource_pack(fb_engine_t* engine,
                                     const char* fbd_file_path);
FB_CAPI int32_t fb_add_resource_pack_data(fb_engine_t* engine,
                                          const uint8_t* fbd_data,
                                          int32_t data_size);
FB_CAPI int32_t fb_set_3d_sticker(fb_engine_t* engine, const char* resource);
FB_CAPI int32_t fb_set_3d_sticker_data(fb_engine_t* engine,
                                       const uint8_t* fbd_data,
                                       int32_t data_size);
FB_CAPI int32_t fb_set_virtual_background(fb_engine_t* engine,
                                          const char* image_path);
FB_CAPI int32_t fb_set_virtual_background_data(fb_engine_t* engine,
                                               const uint8_t* image_data,
                                               int32_t data_size);

/* ------------------------------------------------------------------ */
/* External texture / sync RGBA (TRTC, WASM, pre-encode hooks)        */
/* ------------------------------------------------------------------ */

/**
 * Process a GL_TEXTURE_2D in the calling thread's current GL context.
 *
 * Requirements:
 * - Engine created with external_context = 1
 * - Called on the GL thread that owns the texture (e.g. TRTC onProcessVideoFrame)
 * - `texture` is a valid GL_TEXTURE_2D in that context
 *
 * @param stride     Row bytes; 0 means width * 4
 * @param frame_type fb_frame_type_t
 * @param mirror     fb_mirror_t (texture path does not mirror yet; kept for ABI)
 * @return Processed texture handle, or 0 on failure
 */
FB_CAPI uint32_t fb_process_texture(fb_engine_t* engine,
                                    uint32_t texture,
                                    int32_t width,
                                    int32_t height,
                                    int32_t stride,
                                    int32_t frame_type,
                                    int32_t mirror);

/**
 * Synchronously process a caller-owned RGBA buffer.
 * src / dst are allocated by the caller; they may be the same pointer
 * (in-place is not guaranteed). WASM ImageData paths use this.
 */
FB_CAPI int32_t fb_process_rgba(fb_engine_t* engine,
                                uint8_t* src_data,
                                int32_t src_width,
                                int32_t src_height,
                                int32_t src_stride,
                                uint8_t* dst_data,
                                int32_t frame_type,
                                int32_t mirror);

/* ------------------------------------------------------------------ */
/* Video session                                                      */
/* ------------------------------------------------------------------ */

/**
 * Create a video session with a worker thread and paired slot pools.
 * Prefer one video session per engine; multiple sessions serialize on the GPU.
 * @return Handle on success, NULL on failure.
 */
FB_CAPI fb_session_t* fb_session_create(fb_engine_t* engine,
                                        const fb_session_config_t* config);

/**
 * Destroy a session. Waits for in-flight frames, then invalidates all slot
 * pointers. No other thread may still be reading or writing slots.
 */
FB_CAPI void fb_session_destroy(fb_session_t* session);

/** Register the frame-completion callback. Must be called before the first submit. */
FB_CAPI int32_t fb_session_set_frame_callback(fb_session_t* session,
                                              fb_frame_cb callback,
                                              void* user_data);

/** Total slot count. Input and output counts match and are paired by index. */
FB_CAPI int32_t fb_session_slot_count(fb_session_t* session);

/**
 * Query slot memory layout. Pointers are stable for the session lifetime —
 * cache at init; do not query every frame.
 */
FB_CAPI int32_t fb_session_input_slot_info(fb_session_t* session,
                                           int32_t slot,
                                           fb_slot_info_t* out_info);

FB_CAPI int32_t fb_session_output_slot_info(fb_session_t* session,
                                            int32_t slot,
                                            fb_slot_info_t* out_info);

/**
 * Borrow a free slot (input + paired output). Non-blocking.
 * @return >= 0 slot index, or FB_ERR_NO_SLOT if all slots are in flight.
 *         Drop the frame on NO_SLOT — queuing would only grow latency.
 */
FB_CAPI int32_t fb_session_acquire(fb_session_t* session);

/**
 * Submit a filled slot. Returns immediately; does not wait for processing.
 * After submit the slot belongs to the SDK until fb_frame_cb returns it.
 *
 * @param rotation     fb_rotation_t, clockwise, before processing
 * @param mirror       fb_mirror_t, applied after rotation
 * @param timestamp_us Timestamp echoed into fb_frame_header_t
 * @param user_tag     Custom tag echoed into fb_frame_header_t
 */
FB_CAPI int32_t fb_session_submit(fb_session_t* session,
                                  int32_t slot,
                                  int32_t rotation,
                                  int32_t mirror,
                                  int64_t timestamp_us,
                                  int64_t user_tag);

/**
 * Return a slot. Must pair 1:1 with fb_frame_cb; missing releases exhaust the
 * pool. May also be called after acquire and before submit to abandon a frame.
 */
FB_CAPI int32_t fb_session_release(fb_session_t* session, int32_t slot);

/**
 * Drop all in-flight frames and reset slots (resolution change / pause-resume).
 * Unreleased output slots are force-reclaimed; previous output content is invalid.
 */
FB_CAPI int32_t fb_session_flush(fb_session_t* session);

/* ------------------------------------------------------------------ */
/* Still-image processing                                             */
/* ------------------------------------------------------------------ */

/**
 * Process an image file to an output file asynchronously.
 * @param quality JPEG quality [1, 100]; ignored for PNG
 * @return >= 0 task_id, or a negative error code
 */
FB_CAPI int64_t fb_process_file_async(fb_engine_t* engine,
                                      const char* input_path,
                                      const char* output_path,
                                      int32_t quality,
                                      fb_task_cb callback,
                                      void* user_data);

/**
 * Process an encoded image (png / jpg bytes) and return an RGBA bitmap.
 * `data` is copied before return; the caller need not keep it alive.
 * @return >= 0 task_id, or a negative error code
 */
FB_CAPI int64_t fb_process_encoded_async(fb_engine_t* engine,
                                         const uint8_t* data,
                                         int32_t size,
                                         fb_image_cb callback,
                                         void* user_data);

/** Process an image file and return an RGBA bitmap (no disk write). */
FB_CAPI int64_t fb_process_file_to_bitmap_async(fb_engine_t* engine,
                                                const char* input_path,
                                                fb_image_cb callback,
                                                void* user_data);

/* fb_image_result_t accessors. Data remains valid until release. */
FB_CAPI const uint8_t* fb_image_result_data(fb_image_result_t* result);
FB_CAPI int32_t fb_image_result_size(fb_image_result_t* result);
FB_CAPI int32_t fb_image_result_width(fb_image_result_t* result);
FB_CAPI int32_t fb_image_result_height(fb_image_result_t* result);
FB_CAPI int32_t fb_image_result_stride(fb_image_result_t* result);
FB_CAPI void fb_image_result_release(fb_image_result_t* result);

/* ------------------------------------------------------------------ */
/* Flutter plugin: process-wide live texture engine                   */
/* ------------------------------------------------------------------ */

/**
 * Bind `engine` as the process-wide target of
 * FacebetterPlugin.processTexture / fb_flutter_process_texture.
 *
 * Only an external_context engine should be registered. Returns FB_OK, or
 * FB_ERR_INVALID_ARG if `engine` is null or another live engine is already
 * bound. Re-registering the same pointer is OK.
 *
 * Dart calls this; do not feed frames from Dart.
 */
FB_CAPI int32_t fb_set_flutter_texture_engine(fb_engine_t* engine);

/**
 * Unbind `engine` if it is the current live texture engine (compare-and-swap).
 * Blocks until an in-flight fb_flutter_process_texture returns. Call before
 * fb_engine_destroy. Returns FB_OK, or FB_ERR_NOT_INITIALIZED if this engine
 * is not the bound one.
 */
FB_CAPI int32_t fb_clear_flutter_texture_engine(fb_engine_t* engine);

/** Current live texture engine, or NULL. */
FB_CAPI fb_engine_t* fb_get_flutter_texture_engine(void);

/**
 * Process a GL_TEXTURE_2D on the calling GL thread using the bound live
 * engine. Holds the slot lock for the duration of processing so dispose
 * cannot free the engine mid-frame.
 *
 * @return Output texture id, or 0 if unbound / failure.
 */
FB_CAPI uint32_t fb_flutter_process_texture(uint32_t texture,
                                            int32_t width,
                                            int32_t height,
                                            int32_t stride);

#if defined(__EMSCRIPTEN__)
/**
 * WASM only: write face landmarks into JS-allocated shared memory, then fire a
 * lightweight JS callback. Pass callback_id < 0 or shared_buffer NULL to clear.
 */
FB_CAPI int32_t fb_engine_set_wasm_callbacks(fb_engine_t* engine,
                                             int32_t callback_id,
                                             void* shared_buffer,
                                             int32_t shared_buffer_size);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FACEBETTER_ENGINE_C_FB_C_API_H_ */
