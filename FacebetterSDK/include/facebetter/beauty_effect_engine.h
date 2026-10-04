/*
 * Facebetter Beauty Effect Engine
 *
 * Copyright © 2025 facebetter. All rights reserved.
 */

#pragma once

#include <cstdint>
#include <memory>

#include "facebetter/beauty_params.h"
#include "facebetter/image_frame.h"
#include "facebetter/type_defines.h"

#define FB_SDK_VERSION "2.1.1"

namespace facebetter {

/** Beauty effect engine interface */
class FB_API BeautyEffectEngine {
 public:
  BeautyEffectEngine(const BeautyEffectEngine&) = delete;
  BeautyEffectEngine& operator=(const BeautyEffectEngine&) = delete;

  /**
   * Sets the global log configuration for the SDK.
   * @param config The log configuration to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  static int SetLogConfig(const LogConfig& config);

  /**
   * Creates a new instance of the beauty effect engine.
   * @param config The configuration to initialize the engine with.
   * @return A shared pointer to the created engine instance, or nullptr if
   * creation failed.
   */
  static std::shared_ptr<BeautyEffectEngine> Create(const EngineConfig& config);

  /**
   * Sets skin smoothing intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetSmoothing(float intensity) = 0;

  /**
   * Sets the skin smoothing style.
   * Intensity is still controlled by SetSmoothing.
   * @param style The smoothing style to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetSmoothingStyle(beauty_params::SmoothingStyle style) = 0;

  /**
   * Sets skin whitening intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetWhitening(float intensity) = 0;

  /**
   * Sets the whitening style by swapping the custom LUT.
   * Intensity is still controlled by SetWhitening.
   * @param style The whitening style to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetWhiteningStyle(beauty_params::WhiteningStyle style) = 0;

  /**
   * Sets image sharpening intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetSharpening(float intensity) = 0;

  /**
   * Sets skin rosiness intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetRosiness(float intensity) = 0;

  /**
   * Sets a face-reshaping parameter.
   * @param param The reshape parameter to set.
   * @param intensity Intensity in [-1.0, 1.0]. 0 is off; positive and negative
   *        are opposite directions (see beauty_params::Reshape comments).
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetReshape(beauty_params::Reshape param, float intensity) = 0;

  /**
   * Sets a body-reshaping parameter. Intensity in [0.0, 1.0]; 0 is off.
   * Pose-based params need resource_body.fbd. LegStretch and TorsoLong do not.
   */
  virtual int SetBodyReshape(beauty_params::BodyReshape param,
                             float intensity) = 0;

  /**
   * Sets lipstick intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetLipstick(float intensity) = 0;

  /**
   * Sets the lipstick colour preset. Uses a single lip-mask texture tinted
   * to the preset colour; no per-style PNG swap is required.
   * @param color The lipstick colour to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetLipstickColor(beauty_params::LipstickColor color) = 0;

  /**
   * Sets blush intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetBlush(float intensity) = 0;

  /**
   * Sets the blush style by swapping the mask texture.
   * @param style The blush style to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetBlushStyle(beauty_params::BlushStyle style) = 0;

  /**
   * Sets the blush colour preset. Tints the current shape texture.
   * @param color The blush colour to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetBlushColor(beauty_params::BlushColor color) = 0;

  /**
   * Sets face contour / highlight intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetContour(float intensity) = 0;

  /**
   * Sets the contour / highlight map by swapping the mid-gray texture.
   * @param style The contour style to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetContourStyle(beauty_params::ContourStyle style) = 0;

  /**
   * Sets eyeshadow intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyeShadow(float intensity) = 0;

  /**
   * Sets the eyeshadow style by swapping the mask texture.
   * @param style The eyeshadow style to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyeShadowStyle(beauty_params::EyeShadowStyle style) = 0;

  /**
   * Sets the eyeshadow colour preset. Tints the current shape texture.
   * @param color The eyeshadow colour to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyeShadowColor(beauty_params::EyeShadowColor color) = 0;

  /**
   * Sets eyeliner intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyeLiner(float intensity) = 0;

  /**
   * Sets the eyeliner style by swapping the mask texture.
   * @param style The eyeliner style to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyeLinerStyle(beauty_params::EyeLinerStyle style) = 0;

  /**
   * Sets the eyeliner colour preset. Tints the current shape texture.
   * @param color The eyeliner colour to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyeLinerColor(beauty_params::EyeLinerColor color) = 0;

  /**
   * Sets eyebrow intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyebrow(float intensity) = 0;

  /**
   * Sets the eyebrow style by swapping the mask texture.
   * @param style The eyebrow style to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyebrowStyle(beauty_params::EyebrowStyle style) = 0;

  /**
   * Sets the eyebrow colour preset. Tints the current shape texture.
   * @param color The eyebrow colour to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyebrowColor(beauty_params::EyebrowColor color) = 0;

  /**
   * Sets eyelash intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyelash(float intensity) = 0;

  /**
   * Sets the eyelash style by swapping the mask texture.
   * @param style The eyelash style to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyelashStyle(beauty_params::EyelashStyle style) = 0;

  /**
   * Sets the eyelash colour preset. Tints the current shape texture.
   * @param color The eyelash colour to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetEyelashColor(beauty_params::EyelashColor color) = 0;

  /**
   * Sets colored contact-lens intensity.
   * @param intensity The intensity to apply, usually in range [0.0, 1.0]. 0 is off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetPupil(float intensity) = 0;

  /**
   * Sets the contact-lens colourway by swapping the iris texture.
   * @param color The contact-lens colourway to apply.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetPupilColor(beauty_params::PupilColor color) = 0;

  /**
   * Uses chroma keying (green/blue/red screen) as the virtual-background mask.
   * Fill is still controlled by SetVirtualBackgroundBlur / SetVirtualBackground.
   * With no fill, the keyed region becomes transparent (alpha).
   * Use ClearChromaKey() to switch back to portrait segmentation.
   * @param color Key colour to remove.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetChromaKey(beauty_params::ChromaKeyColor color) = 0;

  /**
   * Turns off chroma keying and restores portrait-segmentation as the mask.
   * Does not clear the current fill (blur / image).
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int ClearChromaKey() = 0;

  /**
   * How close a pixel must be to the key colour to be keyed out. [0.0, 1.0].
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetChromaKeySimilarity(float value) = 0;

  /**
   * Edge feather around the key. [0.0, 1.0].
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetChromaKeySmoothness(float value) = 0;

  /**
   * Spill suppression on semi-transparent edges. [0.0, 1.0].
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetChromaKeyDesaturation(float value) = 0;

  /**
   * Enables virtual background blur.
   * Level is continuous in [0.0, 1.0] (downsample + mix, no shader rebuild).
   * @param level Blur strength in [0.0, 1.0]. 0 clears the virtual background.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetVirtualBackgroundBlur(float level) = 0;

  /**
   * Replaces the background with an image file (png/jpg). Must not be empty.
   * Use ClearVirtualBackground() to turn it off.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetVirtualBackground(const std::string& image_path) = 0;

  /**
   * Replaces the background from encoded image bytes (png/jpg).
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetVirtualBackground(const std::vector<uint8_t>& image_data) = 0;

  /**
   * Clears virtual background (blur or image replacement).
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int ClearVirtualBackground() = 0;

  /**
   * Sets whether to apply beauty effects only on skin regions.
   * When enabled, beauty effects (smoothing, whitening, etc.) will only be
   * applied to detected skin areas, leaving non-skin areas unchanged.
   * @param enabled True to enable skin-only beauty, false to apply to entire image.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetBeautySkinOnly(bool enabled) = 0;

  /**
   * Applies a LUT filter from an .fbd file.
   * The LUT texture is created on the next ProcessImage call (the active GL
   * thread / external context). Use ClearFilter() to remove the current filter.
   * @param fbd_file_path Path to the filter .fbd file. Must not be empty.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetFilter(const std::string& fbd_file_path) = 0;

  /**
   * Applies a LUT filter from an in-memory .fbd buffer (e.g. Android assets).
   * The LUT texture is created on the next ProcessImage call.
   * @param fbd_data The .fbd file bytes. Must not be empty.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetFilter(const std::vector<uint8_t>& fbd_data) = 0;

  /**
   * Clears the current LUT filter.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int ClearFilter() = 0;

  /**
   * Sets the intensity of the current filter.
   * @param intensity The intensity value [0.0, 1.0].
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual   int SetFilterIntensity(float intensity) = 0;

  /**
   * Registers an extra capability resource pack (.fbd). The engine looks up
   * models by inner path in the core `resource.fbd` plus every pack added
   * here. Later packs override earlier ones on the same inner path. Call
   * before the feature that needs the files (e.g. Set3DSticker).
   * @param fbd_file_path Path to an extra .fbd pack. Must not be empty.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int AddResourcePack(const std::string& fbd_file_path) = 0;

  /**
   * Registers an extra capability resource pack from memory.
   * @param fbd_data The .fbd file bytes. Must not be empty.
   */
  virtual int AddResourcePack(const std::vector<uint8_t>& fbd_data) = 0;

  /**
   * Applies a 2D sticker from an .fbd file.
   * Textures are created on the next ProcessImage call (the active GL thread /
   * external context). Use ClearSticker() to remove the current sticker.
   * @param fbd_file_path Path to the sticker .fbd file. Must not be empty.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetSticker(const std::string& fbd_file_path) = 0;

  /**
   * Applies a 2D sticker from an in-memory .fbd buffer (e.g. Android assets).
   * Textures are created on the next ProcessImage call.
   * @param fbd_data The .fbd file bytes. Must not be empty.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetSticker(const std::vector<uint8_t>& fbd_data) = 0;

  /**
   * Clears the current 2D sticker.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int ClearSticker() = 0;

  /**
   * Applies a 3D sticker from a .fbd pack (sticker3d.json + mesh.obj), the
   * same way SetSticker() takes a 2D sticker .fbd. Use Clear3DSticker() to
   * remove.
   */
  virtual int Set3DSticker(const std::string& resource) = 0;

  /**
   * Applies a 3D sticker from an in-memory .fbd buffer (e.g. Android assets).
   */
  virtual int Set3DSticker(const std::vector<uint8_t>& fbd_data) = 0;

  /**
   * Clears the current 3D sticker.
   */
  virtual int Clear3DSticker() = 0;

  /**
   * Gets the engine performance statistics.
   * @return The current engine statistics including FPS and average process time.
   */
  virtual EngineStats GetStats() const = 0;

  /**
   * Sets the engine callbacks for events and data.
   * @param callbacks The collection of callbacks to register.
   * @return 0 on success, non-zero error code otherwise.
   */
  virtual int SetCallbacks(const EngineCallbacks& callbacks) = 0;

  /**
   * Processes an image frame with the currently configured beauty effects.
   * @param image_frame The input image frame to process.
   * @return A shared pointer to the processed image frame.
   */
  virtual const std::shared_ptr<ImageFrame> ProcessImage(
      const std::shared_ptr<ImageFrame> image_frame) = 0;

 protected:
  BeautyEffectEngine() = default;
  virtual ~BeautyEffectEngine();
};

}  // namespace facebetter
