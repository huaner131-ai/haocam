//
//  Facebetter Beauty Effect SDK
//
//  Copyright (c) 2015 facebetter. All rights reserved.
//
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
// Windows (32-bit and 64-bit)
#ifdef _WIN64
// Windows (64-bit only)
#define FB_PLATFORM_WIN
#else
// Windows (32-bit only)
#define FB_PLATFORM_WIN
#endif
#elif __APPLE__
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR
// iOS, tvOS, or watchOS Simulator
#define FB_PLATFORM_IOS
#elif TARGET_OS_MACCATALYST
// Mac's Catalyst (ports iOS API into Mac, like UIKit).
#define FB_PLATFORM_IOS
#elif TARGET_OS_IPHONE
// iOS, tvOS, or watchOS device
#define FB_PLATFORM_IOS
#elif TARGET_OS_MAC
// Other kinds of Apple platforms
#define FB_PLATFORM_MAC
#else
#error "Unknown Apple platform"
#endif
#elif __ANDROID__
// Android
#define FB_PLATFORM_ANDROID
#elif __linux__
// Linux
#define FB_PLATFORM_LINUX
#elif __EMSCRIPTEN__
// WebAssembly
#define FB_PLATFORM_WASM
#elif defined(_POSIX_VERSION)
// POSIX
#error "Unknown platform"
#define FB_PLATFORM_POSIX
#else
#error "Unknown platform"
#endif

// API export/import macros
#ifndef FB_API
#if defined(FB_PLATFORM_WIN)
#define FB_API __declspec(dllexport)
#else
#define FB_API __attribute__((visibility("default")))
#endif
#endif

namespace facebetter {
/** Log levels */
enum class LogLevel {
  Trace = 0,  // Trace level
  Debug,      // Debug level
  Info,       // Info level
  Warn,       // Warn level
  Error,      // Error level
  Critical    // Critical error level
};

/** Log configuration structure */
struct LogConfig {
  bool console_enabled = false;
  bool file_enabled = false;
  LogLevel level = LogLevel::Info;
  std::string file_name = "";
};

// Pixel format definitions for unified use in ImageBuffer and ImageFrame.
enum class Format {
  I420,     // YUV 4:2:0 12bpp (3 planes: Y, U, V)
  NV12,     // YUV 4:2:0 12bpp (2 planes: Y + UV)
  NV21,     // YUV 4:2:0 12bpp (2 planes: Y + VU, Android default)
  BGRA,     // BGRA 8:8:8:8 32bpp (4 channels)
  RGBA,     // RGBA 8:8:8:8 32bpp (4 channels)
  BGR,      // BGR 8:8:8 24bpp (3 channels)
  RGB,      // RGB 8:8:8 24bpp (3 channels)
  Texture,  // Texture format (used for GPU textures)
};

// Rotation angle definitions for all image buffers and frames.
enum class Rotation {
  Rotation_0,    // 0 degrees
  Rotation_90,   // 90 degrees clockwise
  Rotation_180,  // 180 degrees clockwise
  Rotation_270,  // 270 degrees clockwise
};

/** Image frame type */
enum class FrameType {
  Image = 0,  // Image mode
  Video = 1   // Video mode
};

/**
 * Engine configuration.
 *
 * License validation priority:
 * 1. If license_token is non-empty, verify that JWS (v2 online token or native
 *    offline .lic).
 * 2. Native: otherwise use app_id + app_key against /facebetter/v2/auth.
 * 3. Web: license_token is required. Fetch the token outside the engine
 *    (your server, or a local debug fetch) and pass it in. The engine never
 *    contacts the auth API from Web.
 */
struct EngineConfig {
  std::string app_id;   // Native online auth (with app_key). Do not use on Web.
  std::string app_key;  // Native online auth. Do not embed in browser JS.
  std::string resource_path;  // Directory containing resource.fbd
  std::string license_token;  // Compact JWS, HTTP {token} envelope, or offline .lic
  // External OpenGL context:
  // - true: do not create/switch an internal GL context; all GPU work uses the
  //         caller's current context on the calling thread.
  // - false: use the default internal context and task queue.
  bool external_context = false;
};

/** 2D point coordinates */
struct Point2d {
  float x; /**< X coordinate (normalized [0.0, 1.0]) */
  float y; /**< Y coordinate (normalized [0.0, 1.0]) */
};

/** Rectangular area */
struct Rect {
  float x;      /**< Top-left x coordinate (normalized [0.0, 1.0]) */
  float y;      /**< Top-left y coordinate (normalized [0.0, 1.0]) */
  float width;  /**< Width (normalized [0.0, 1.0]) */
  float height; /**< Height (normalized [0.0, 1.0]) */
};

/** Face detection result */
struct FaceDetectionResult {
  /** Face bounding box (normalized [0.0, 1.0]) */
  Rect rect = {0.0f, 0.0f, 0.0f, 0.0f};

  /** 111 normalized face landmarks */
  std::vector<Point2d> key_points = {};

  /** Per-landmark visibility scores [0.0, 1.0] */
  std::vector<float> visibility = {};

  /** Stable face id within the tracking session (-1 if unknown) */
  int face_id = -1;

  /** Face confidence for this frame [0.0, 1.0] */
  float score = 0.0f;

  /** Pitch angle in radians: up -, down +. Range [-Pi, Pi] */
  float pitch = 0.0f;

  /** Roll angle in radians: left -, right +. Range [-Pi, Pi] */
  float roll = 0.0f;

  /** Yaw angle in radians: left -, right +. Range [-Pi, Pi] */
  float yaw = 0.0f;
};

/**
 * Engine event codes
 * 0: Success event
 * Non-zero: Failure event, specific reason described in message field
 */
enum class EngineEventCode {
  // License validation events
  LicenseValidationSuccess = 0,  // License validation successful
  LicenseValidationFailed =
      1,  // License validation failed (details in message)

  // Engine initialization events (100+ range)
  EngineInitializationComplete =
      100,  // Engine initialization complete (filters initialized)
  EngineInitializationFailed = 101,  // Engine initialization failed

  // Reserved 200+ range for future extension

};

/** Collection of engine callback functions */
struct EngineCallbacks {
  /**
   * Face landmark detection callback
   * @param results List of detected face landmark results
   */
  std::function<void(const std::vector<FaceDetectionResult>& results)>
      on_face_landmarks = nullptr;

  /**
   * Engine event callback
   * @param code Event code (see EngineEventCode)
   * @param message Event details (e.g., "success" or error description)
   */
  std::function<void(int code, const std::string& message)> on_engine_event =
      nullptr;
};

/** Engine performance statistics */
struct EngineStats {
  double fps = 0.0;              /**< Current frames per second */
  double avg_process_time_ms = 0.0;  /**< Average ProcessImage time in milliseconds */
  double session_time_s = 0.0;  /**< Session duration in seconds */
};

}  // namespace facebetter
