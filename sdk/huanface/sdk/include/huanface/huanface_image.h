/**
 * HuanFace Image Loader — Phase 4
 * P0: Load/Save RGBA8 PNG/JPG/BMP via minimal implementation
 * PNG via zlib (real), BMP via simple parser, JPG via stub (not implemented minimal)
 * Clean-room, no proprietary
 */

#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace huanface {

struct HFImage {
    int width = 0;
    int height = 0;
    int channels = 4; // RGBA8
    std::vector<uint8_t> data; // RGBA8, size = w*h*4

    bool IsValid() const { return width > 0 && height > 0 && data.size() == (size_t)width*height*4; }
    uint8_t* Pixel(int x, int y) { return &data[(y*width + x)*4]; }
    const uint8_t* Pixel(int x, int y) const { return &data[(y*width + x)*4]; }
};

class ImageLoader {
public:
    // Load image from file (PNG, BMP supported, JPG returns error for minimal)
    static bool LoadImage(const std::string& path, HFImage& outImage, std::string& outError);
    static bool LoadImageFromMemory(const uint8_t* data, size_t size, HFImage& outImage, std::string& outError);

    // Save image as PNG (RGBA8)
    static bool SaveImage(const std::string& path, const HFImage& image, std::string& outError);
    static bool SaveImageAsPNG(const std::string& path, int width, int height, const uint8_t* rgbaData, std::string& outError);
    static bool EncodePNGToMemory(int width, int height, const uint8_t* rgbaData, std::vector<uint8_t>& outPng, std::string& outError);

    // Helpers
    static bool IsPNG(const uint8_t* data, size_t size);
    static bool IsBMP(const uint8_t* data, size_t size);
    static bool IsJPG(const uint8_t* data, size_t size);
};

} // namespace huanface
