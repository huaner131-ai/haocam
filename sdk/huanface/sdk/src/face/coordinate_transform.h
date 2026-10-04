/**
 * HuanFace Coordinate Transform — Phase 5
 * Utilities for pixel/normalized/mirrored/rotated/D3D11 conversions
 */

#pragma once
#include "face_data.h"

namespace huanface {

class CoordinateTransform {
public:
    // Pixel <-> Normalized
    static HFVec2 PixelToNormalized(const HFVec2& pixel, int width, int height) {
        if (width <= 0 || height <= 0) return HFVec2(0,0);
        return HFVec2(pixel.x / (float)width, pixel.y / (float)height);
    }
    static HFVec2 NormalizedToPixel(const HFVec2& norm, int width, int height) {
        return HFVec2(norm.x * width, norm.y * height);
    }
    static HFVec2UV PixelToUV(const HFVec2& pixel, int width, int height) {
        return HFVec2UV(pixel.x / (float)width, pixel.y / (float)height);
    }
    static HFVec2 UVToPixel(const HFVec2UV& uv, int width, int height) {
        return HFVec2(uv.u * width, uv.v * height);
    }

    // Normalized -> D3D11 NDC
    static HFVec2 UVToD3D11NDC(const HFVec2UV& uv) {
        return HFVec2(uv.u*2.0f-1.0f, 1.0f-uv.v*2.0f);
    }
    static HFVec2 NormalizedToD3D11NDC(const HFVec2& norm) {
        return HFVec2(norm.x*2.0f-1.0f, 1.0f-norm.y*2.0f);
    }

    // Mirror horizontal
    static HFVec2 MirrorHorizontal(const HFVec2& pt, int imageWidth) {
        return HFVec2((float)imageWidth - 1 - pt.x, pt.y);
    }
    static HFVec2UV MirrorHorizontalUV(const HFVec2UV& uv) {
        return HFVec2UV(1.0f - uv.u, uv.v);
    }

    // Rotation 0/90/180/270 clockwise
    static HFVec2 RotatePoint(const HFVec2& pt, int width, int height, int rotation) {
        switch (rotation) {
            case 0: return pt;
            case 90: return HFVec2((float)height - 1 - pt.y, pt.x);
            case 180: return HFVec2((float)width - 1 - pt.x, (float)height - 1 - pt.y);
            case 270: return HFVec2(pt.y, (float)width - 1 - pt.x);
            default: return pt;
        }
    }

    // Rotate bbox
    static void RotateBBox(float& x, float& y, float& w, float& h, int imgW, int imgH, int rotation) {
        HFVec2 tl(x, y);
        HFVec2 br(x+w, y+h);
        HFVec2 tlR = RotatePoint(tl, imgW, imgH, rotation);
        HFVec2 brR = RotatePoint(br, imgW, imgH, rotation);
        // After rotation, width/height may swap for 90/270
        float newX = std::min(tlR.x, brR.x);
        float newY = std::min(tlR.y, brR.y);
        float newW = std::abs(brR.x - tlR.x);
        float newH = std::abs(brR.y - tlR.y);
        x = newX; y = newY; w = newW; h = newH;
    }

    // Transform landmarks with mirror/rotation
    static void TransformLandmarks(std::vector<HFVec2>& landmarks, int imgW, int imgH, int rotation, bool mirrored) {
        for (auto& lm : landmarks) {
            if (mirrored) {
                lm = MirrorHorizontal(lm, imgW);
            }
            if (rotation != 0) {
                lm = RotatePoint(lm, imgW, imgH, rotation);
            }
        }
    }

    // Landmark to mesh vertex conversion
    static HFVec3 LandmarkToMeshVertex(const HFVec2& lm, float depth, int imgW, int imgH) {
        (void)imgW; (void)imgH;
        return HFVec3(lm.x, lm.y, depth);
    }
};

} // namespace huanface
