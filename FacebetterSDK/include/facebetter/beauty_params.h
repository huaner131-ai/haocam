//
//  Facebetter Beauty Effect SDK
//
//  Copyright (c) 2015 facebetter. All rights reserved.
//
#pragma once

namespace facebetter {

/** Beauty parameter categories */
namespace beauty_params {
/** Whitening styles (swaps the custom 512 LUT) */
enum class WhiteningStyle {
  ColdWhite = 0,  // Cool white
  PinkWhite,      // Pink white
  WarmWhite,      // Warm white
  Wheat,          // Light wheat / olive
  Tan,            // Tan / bronzed
};

/** Skin smoothing styles (same blur, different mix / texture restore) */
enum class SmoothingStyle {
  Natural = 0,  // Natural: keeps pores
  Texture,      // Cleaner skin while retaining texture
  Smooth,       // Creamy / porcelain finish
};

/**
 * Face reshaping parameters.
 *
 * Intensity range is [-1.0, 1.0]; 0 leaves the feature unchanged. Comments list
 * the positive / negative directions. Passing only [0.0, 1.0] keeps the legacy
 * one-sided behaviour.
 *
 * All reshape effects share one landmark set and displacement field. Magnitudes
 * are scaled by inter-ocular distance so the same value looks consistent across
 * resolutions, face distance, and aspect ratios.
 *
 * Enum ordinals are part of the ABI: values introduced in 1.1.3 must keep their
 * indices; new parameters are always appended.
 */
enum class Reshape {
  FaceThin = 0,     // +slim face / -fuller cheeks
  FaceVShape = 1,   // +V-shape jaw / -square jaw
  FaceNarrow = 2,   // +narrow face / -wider face
  FaceShort = 3,    // +shorter face / -longer face
  Cheekbone = 4,    // +slim cheekbones / -wider cheekbones
  Jawbone = 5,      // +slim jaw / -wider jaw
  Chin = 6,         // +longer chin / -shorter chin
  NoseSlim = 7,     // +slimmer nose / -wider nose
  EyeSize = 8,      // +larger eyes / -smaller eyes
  EyeDistance = 9,  // +wider eye spacing / -closer eyes

  FaceSmall = 10,      // +smaller face / -larger face
  Forehead = 11,       // +fuller forehead / -lower forehead
  NoseLong = 12,       // +longer nose / -shorter nose
  Philtrum = 13,       // +shorter philtrum / -longer philtrum
  MouthSize = 14,      // +larger mouth / -smaller mouth
  MouthPosition = 15,  // +mouth lower / -mouth higher
  MouthSmile = 16,     // +smile lift / -droop corners
  LipThickness = 17,   // +thicker lips / -thinner lips
  EyeRound = 18,       // +rounder eyes / -narrower eyes
  EyePosition = 19,    // +eyes lower / -eyes higher
  EyeAngle = 20,       // +outer corner up / -outer corner down
  EyeCornerOpen = 21,  // +open eye corners / -close eye corners
  LowerEyelid = 22,    // +lower eyelid down / -lift lower eyelid
  BrowPosition = 23,   // +brows higher / -brows lower
  BrowDistance = 24,   // +wider brow spacing / -closer brows
  BrowThickness = 25,  // +thicker brows / -thinner brows
};

/**
 * Body reshaping parameters. Intensity is [0.0, 1.0]; 0 is off.
 * Pose-based params need optional resource_body.fbd beside resource.fbd.
 * LegStretch and TorsoLong use face landmarks only.
 *
 * Enum ordinals are part of the ABI; new parameters are always appended.
 */
enum class BodyReshape {
  BodySlim = 0,      // Slim torso
  WaistSlim = 1,     // Slim waist
  LegSlim = 2,       // Slim legs
  ShoulderSlim = 3,  // Slim / narrow shoulders
  ArmSlim = 4,       // Slim arms
  LegLong = 5,       // Lengthen legs along the skeleton (needs body pose)
  BustEnhance = 6,   // Bust fullness (front + side)
  LegStretch = 7,    // Lengthen legs by stretching below the waist (face only)
  TorsoLong = 8,     // Lengthen the torso between chin and waist (face only)
};

/** Contour / highlight map styles (one mid-gray Overlay texture per style) */
enum class ContourStyle {
  Natural = 0,  // Soft everyday contour
  Sculpt,       // Deeper sculpted contour
  Glow,         // Highlight-focused
  Slim,         // Slim cheekbones
  Nose,         // Nose bridge lift
  Glam,         // Spot highlights
};

/** Lipstick colour presets (single alpha-mask texture + tint) */
enum class LipstickColor {
  Rouge = 0,       // Classic rose
  RetroRed,        // Retro red
  Peach,           // Peach
  CoralOrange,     // Coral orange
  GentlePink,      // Soft pink
  VitalityOrange,  // Bright orange
};

/** Blush styles (one full-face mask texture per style) */
enum class BlushStyle {
  SunKissed = 0,  // Sun-kissed sweep across cheekbones and bridge
  Igari,          // Flushed band across the nose bridge
  Soft,           // Soft dual cheeks
  Apple,          // Round apple cheeks
  Classic,        // Classic two spots
  Doll,           // Cheeks + nose tip
  Rose,           // Rose dual cheeks
};

/** Blush colour presets (tint applied to the current shape texture) */
enum class BlushColor {
  CoralPink = 0,  // Coral pink
  DustyRose,      // Dusty rose
  VividRed,       // Vivid red
  Berry,          // Berry
  SunsetOrange,   // Sunset orange
};

/** Eyeshadow styles (one cropped eye-band mask per style) */
enum class EyeShadowStyle {
  Soft = 0,  // Soft wash
  Crease,    // Crease deepen
  Smoky,     // Smoky surround
  Halo,      // Halo around the eye
  Glow,      // Outer-corner glow
  Drama,     // Full dramatic cover
  Warm,      // Warm-tone wash
};

/** Eyeshadow colour presets */
enum class EyeShadowColor {
  Plum = 0,  // Plum / mauve (default)
  Brown,     // Warm brown
  Gold,      // Soft gold
  Pink,      // Dusty pink
};

/** Eyeliner styles (one cropped eye-band mask per style) */
enum class EyeLinerStyle {
  Classic = 0,  // Full liner with winged tip
  Flick,        // Extended outer flick
  CatEye,       // Short cat-eye lift
  Natural,      // Thin soft natural line
  Bold,         // Bold cover
  Soft,         // Soft feathered wing
};

/** Eyeliner colour presets (tint applied to the current shape texture) */
enum class EyeLinerColor {
  Burgundy = 0,  // Burgundy brown
  Plum,          // Deep plum
  Chocolate,     // Chocolate brown
  Coffee,        // Near-black coffee
  Mauve,         // Mauve grey
};

/** Eyebrow styles (one cropped brow-band mask per style) */
enum class EyebrowStyle {
  Natural = 0,  // Natural arch with hair strokes
  Soft,         // Soft powdery thick brow
  Feathered,    // Feathered hair strokes
  Mist,         // Light powder mist
  Arched,       // Classic high arch
  Powder,       // Dense powder fill
  Wild,         // Wispy upper edge
  Full,         // Full balanced brow
  Straight,     // Straight low arch
};

/** Eyebrow colour presets */
enum class EyebrowColor {
  DarkBrown = 0,  // Dark brown (default)
  Black,          // Black
  SoftBrown,      // Soft brown
};

/** Eyelash styles (one cropped eye-band mask per style) */
enum class EyelashStyle {
  Classic = 0,  // Balanced clusters + lower lashes
  Manga,        // Bold spiked clusters
  Winged,       // Extended outer corner
  Wispy,        // Fluffy lifted tips
  Clustered,    // Distinct clusters
  Doll,         // Short doll lashes
};

/** Eyelash colour presets */
enum class EyelashColor {
  Black = 0,  // Near-black (default)
  Brown,      // Soft brown
  SoftBlack,  // Slightly softer black
};

/** Contact-lens colourways (one centered iris disc per style, no tint) */
enum class PupilColor {
  Hazel = 0,  // Amber / honey brown
  Ice,        // Icy blue
  Mocha,      // Dark brown with sparkle
  Olive,      // Forest green
  Gloss,      // Glossy dark brown
  Moss,       // Muted olive
  Sand,       // Sandy beige brown
  Glow,       // Lower-iris crescent highlight
  Slate,      // Cool blue-grey
};

/** Chroma key colour (mask source for virtual background) */
enum class ChromaKeyColor {
  Green = 0,
  Blue,
  Red,
};

}  // namespace beauty_params

}  // namespace facebetter
