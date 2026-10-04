//
//  Facebetter Beauty Effect SDK
//
//  Copyright (c) 2015 facebetter. All rights reserved.
//
#pragma once

#include <cstdint>
#include <string>

#include "facebetter/type_defines.h"

namespace facebetter {
class ImageBuffer;

/** Image frame class, providing functions for creating, converting and
 * operating image data.
 *
 * Design conventions:
 * - ImageFrame holds a frame of image data through ImageBuffer internally.
 * - Internal working formats are only RGBA / I420 / Texture, determined by the
 * underlying pipeline.
 * - The Engine usually tries to keep the output frame format consistent with
 * the input frame format, but intermediate frames or internal usage scenarios
 * directly expose the internal working format.
 *
 * Thread safety:
 * - Unless the caller implements their own locking, ImageFrame instances do not
 * guarantee thread safety.
 * - It is not recommended to call Rotate / ToXXX operations on the same
 * ImageFrame simultaneously from multiple threads.
 */
class FB_API ImageFrame {
 public:
  ImageFrame(const ImageFrame&) = delete;
  ImageFrame& operator=(const ImageFrame&) = delete;

  /**
   * Creates an ImageFrame from Android Camera2 YUV420 data.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param yBuffer The Y plane data.
   * @param strideY The stride of the Y plane.
   * @param uBuffer The U plane data.
   * @param strideU The stride of the U plane.
   * @param vBuffer The V plane data.
   * @param strideV The stride of the V plane.
   * @param pixelStrideUV The pixel stride for UV planes.
   * @return A shared pointer to the created ImageFrame.
   */
  static std::shared_ptr<ImageFrame> CreateWithAndroid420(int width,
                                                          int height,
                                                          uint8_t* yBuffer,
                                                          int strideY,
                                                          uint8_t* uBuffer,
                                                          int strideU,
                                                          uint8_t* vBuffer,
                                                          int strideV,
                                                          int pixelStrideUV);

  /**
   * General creation method (recommended only for single-plane formats: RGBA /
   * BGRA / RGB / BGR). For multi-plane YUV formats (I420/NV12/NV21/Android420),
   * it is recommended to use the corresponding CreateWithI420 / CreateWithNV12
   * / CreateWithNV21 / CreateWithAndroid420 interfaces to avoid implicit
   * assumptions about continuous memory layout.
   * @param data The raw image data.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param format The pixel format of the data.
   * @return A shared pointer to the created ImageFrame.
   */
  static std::shared_ptr<ImageFrame> Create(const uint8_t* data,
                                            int width,
                                            int height,
                                            Format format);

  /**
   * Creates an ImageFrame from a GPU texture.
   * @param texture The OpenGL texture handle.
   * @param width The width of the texture.
   * @param height The height of the texture.
   * @param stride The stride of the texture data.
   * @return A shared pointer to the created ImageFrame.
   */
  static std::shared_ptr<ImageFrame> CreateWithTexture(uint32_t texture,
                                                       int width,
                                                       int height,
                                                       int stride);

  /**
   * Creates an ImageFrame from RGBA data.
   * @param data The RGBA pixel data.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param stride The stride of the image data.
   * @return A shared pointer to the created ImageFrame.
   */
  static std::shared_ptr<ImageFrame> CreateWithRGBA(const uint8_t* data,
                                                    int width,
                                                    int height,
                                                    int stride);

  /**
   * Creates an ImageFrame from BGRA data.
   * @param data The BGRA pixel data.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param stride The stride of the image data.
   * @return A shared pointer to the created ImageFrame.
   */
  static std::shared_ptr<ImageFrame> CreateWithBGRA(const uint8_t* data,
                                                    int width,
                                                    int height,
                                                    int stride,
                                                    bool copy_data = false);

  /**
   * Creates an ImageFrame from RGB data.
   * @param data The RGB pixel data.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param stride The stride of the image data.
   * @return A shared pointer to the created ImageFrame.
   */
  static std::shared_ptr<ImageFrame> CreateWithRGB(const uint8_t* data,
                                                   int width,
                                                   int height,
                                                   int stride);

  /**
   * Creates an ImageFrame from BGR data.
   * @param data The BGR pixel data.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param stride The stride of the image data.
   * @return A shared pointer to the created ImageFrame.
   */
  static std::shared_ptr<ImageFrame> CreateWithBGR(const uint8_t* data,
                                                   int width,
                                                   int height,
                                                   int stride);

  /**
   * Creates an ImageFrame from I420 (YUV 4:2:0) data.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param dataY The Y plane data.
   * @param strideY The stride of the Y plane.
   * @param dataU The U plane data.
   * @param strideU The stride of the U plane.
   * @param dataV The V plane data.
   * @param strideV The stride of the V plane.
   * @return A shared pointer to the created ImageFrame.
   */
  static std::shared_ptr<ImageFrame> CreateWithI420(int width,
                                                    int height,
                                                    const uint8_t* dataY,
                                                    int strideY,
                                                    const uint8_t* dataU,
                                                    int strideU,
                                                    const uint8_t* dataV,
                                                    int strideV);

  /**
   * Creates an ImageFrame from NV12 (YUV 4:2:0) data.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param dataY The Y plane data.
   * @param strideY The stride of the Y plane.
   * @param dataUV The interleaved UV plane data.
   * @param strideUV The stride of the UV plane.
   * @return A shared pointer to the created ImageFrame.
   */
  static std::shared_ptr<ImageFrame> CreateWithNV12(int width,
                                                    int height,
                                                    const uint8_t* dataY,
                                                    int strideY,
                                                    const uint8_t* dataUV,
                                                    int strideUV);

  /**
   * Creates an ImageFrame from NV21 (YUV 4:2:0) data.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param dataY The Y plane data.
   * @param strideY The stride of the Y plane.
   * @param dataUV The interleaved VU plane data.
   * @param strideUV The stride of the VU plane.
   * @return A shared pointer to the created ImageFrame.
   */
  static std::shared_ptr<ImageFrame> CreateWithNV21(int width,
                                                    int height,
                                                    const uint8_t* dataY,
                                                    int strideY,
                                                    const uint8_t* dataUV,
                                                    int strideUV);

  /**
   * Creates an ImageFrame from an image file.
   * @param file_path The path to the image file.
   * @return A shared pointer to the created ImageFrame, or nullptr if failed.
   */
  static std::shared_ptr<ImageFrame> CreateWithFile(
      const std::string& file_path);

  /**
   * Creates an ImageFrame from an existing ImageBuffer.
   * @param buffer The underlying image buffer.
   * @return A shared pointer to the created ImageFrame.
   */
  static std::shared_ptr<ImageFrame> CreateFromBuffer(
      const std::shared_ptr<ImageBuffer>& buffer);

  /**
   * Rotates the image frame.
   * @param rotation The rotation angle to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  int Rotate(Rotation rotation);

  /**
   * Mirrors the image frame. Allowed mode values (case-insensitive):
   * "horizontal" - left-right mirror, "vertical" - top-bottom mirror,
   * "both" - horizontal and vertical (equivalent to 180-degree rotation).
   * @param mode One of "horizontal", "vertical", "both".
   * @return 0 on success, non-zero error code on invalid mode or failure.
   */
  int Mirror(const std::string& mode);

  /**
   * Sets the mirror mode for engine processing. When this frame is passed to
   * BeautyEffectEngine::ProcessImage(), the engine will mirror the image after
   * converting to internal format (I420/RGBA), avoiding extra format
   * round-trips. Allowed mode values (case-insensitive): "horizontal",
   * "vertical", "both". Empty string clears mirror.
   * @param mode One of "horizontal", "vertical", "both", or "" to clear.
   */
  void SetMirror(const std::string& mode);

  /**
   * Whether horizontal mirror is set (for engine use).
   */
  bool MirrorHorizontal() const { return mirror_horizontal_; }

  /**
   * Whether vertical mirror is set (for engine use).
   */
  bool MirrorVertical() const { return mirror_vertical_; }

  /**
   * Gets the width of the image.
   * @return The image width.
   */
  int32_t Width() const;

  /**
   * Gets the height of the image.
   * @return The image height.
   */
  int32_t Height() const;

  /**
   * Gets the row stride of the image.
   * @return The row stride.
   */
  int32_t Stride() const;

  /**
   * Gets the total size of the image data in bytes.
   * @return The data size.
   */
  int32_t Size() const;

  /**
   * Gets a pointer to the raw image data.
   * @return A pointer to the pixel data.
   */
  const uint8_t* Data() const;

  /**
   * Gets a pointer to the Y plane data (YUV formats only).
   * @return A pointer to the Y data, or nullptr if not a YUV format.
   */
  const uint8_t* DataY() const;

  /**
   * Gets a pointer to the U plane data (YUV formats only).
   * @return A pointer to the U data, or nullptr if not a YUV format.
   */
  const uint8_t* DataU() const;

  /**
   * Gets a pointer to the V plane data (YUV formats only).
   * @return A pointer to the V data, or nullptr if not a YUV format.
   */
  const uint8_t* DataV() const;

  /**
   * Gets the row stride of the Y plane (YUV formats only).
   * @return The Y plane stride.
   */
  int32_t StrideY() const;

  /**
   * Gets the row stride of the U plane (YUV formats only).
   * @return The U plane stride.
   */
  int32_t StrideU() const;

  /**
   * Gets the row stride of the V plane (YUV formats only).
   * @return The V plane stride.
   */
  int32_t StrideV() const;

  /**
   * Gets a pointer to the UV plane data (NV12/NV21 formats only).
   * @return A pointer to the UV data, or nullptr if not a relevant format.
   */
  const uint8_t* DataUV() const;

  /**
   * Gets the row stride of the UV plane (NV12/NV21 formats only).
   * @return The UV plane stride.
   */
  int32_t StrideUV() const;

  /**
   * Gets the pixel format of the image.
   * @return The format enum.
   */
  Format GetFormat() const;

  /**
   * Gets the OpenGL texture handle (Texture format only).
   * @return The texture handle, or 0 if not a texture format.
   */
  uint32_t Texture() const;

  // Frame type (image mode or video mode)
  FrameType type = FrameType::Video;

  // Returns the primary image buffer of the current frame.
  //
  // Semantic explanation:
  // - For Engine input/output:
  //   - Input frame: Buffer() format is the format specified during
  //   construction (e.g., NV12 / I420 / RGBA).
  //   - Output frame: Engine tries to automatically convert the internal
  //   working format back to match the input format,
  //     so Buffer()->GetFormat() usually equals the input format.
  // - For internal intermediate frames (e.g., detection frames, segmentation
  // masks), Buffer()
  //   directly returns the internal working format (usually RGBA, I420, or
  //   Texture).
  //
  // Callers needing to distinguish specific formats should always use
  // GetFormat():
  //   auto buf = frame->Buffer();
  //   if (buf && buf->GetFormat() == Format::NV12) { ... }
  std::shared_ptr<ImageBuffer> Buffer() const { return image_buffer_; }

  // Converts the current frame to the specified pixel format and returns a new
  // ImageFrame. If the current format is the same as the target format, an
  // equivalent frame is returned (sharing the underlying buffer). Returns
  // nullptr if conversion is not possible.
  std::shared_ptr<ImageFrame> Convert(Format format) const;
  int ToFile(const std::string& path, int quality = 90) const;

 private:
  ImageFrame() = default;
  ~ImageFrame() = default;

  std::shared_ptr<ImageBuffer> image_buffer_;
  bool mirror_horizontal_ = false;
  bool mirror_vertical_ = false;
};

}  // namespace facebetter
