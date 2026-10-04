/**
 * HuanFace Face Mask Generator Implementation — Phase 4
 * Triangle rasterization R8, FeatureMask Lip
 */

#include "face_mask.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace huanface {

bool FaceMaskGenerator::PointInTriangle(float px, float py, const HFVec2& v0, const HFVec2& v1, const HFVec2& v2) {
    // Barycentric technique
    float dX = px - v2.x;
    float dY = py - v2.y;
    float dX21 = v2.x - v1.x;
    float dY12 = v1.y - v2.y;
    float D = dY12*(v0.x - v2.x) + dX21*(v0.y - v2.y);
    if (fabs(D) < 1e-6f) return false;
    float s = dY12*dX + dX21*dY;
    s /= D;
    float t = (v2.y - v0.y)*dX + (v0.x - v2.x)*dY;
    t /= D;
    if (s < 0 || t < 0 || s+t > 1) return false;
    return true;
}

void FaceMaskGenerator::RasterizeTriangle(const HFVec2& v0, const HFVec2& v1, const HFVec2& v2, FaceMask& mask, uint8_t value) {
    if (!mask.IsValid()) return;
    // Bounding box
    int minX = (int)std::floor(std::min({v0.x, v1.x, v2.x}));
    int maxX = (int)std::ceil(std::max({v0.x, v1.x, v2.x}));
    int minY = (int)std::floor(std::min({v0.y, v1.y, v2.y}));
    int maxY = (int)std::ceil(std::max({v0.y, v1.y, v2.y}));
    minX = std::max(0, minX);
    maxX = std::min(mask.width-1, maxX);
    minY = std::max(0, minY);
    maxY = std::min(mask.height-1, maxY);

    for (int y=minY; y<=maxY; ++y) {
        for (int x=minX; x<=maxX; ++x) {
            // Use pixel center
            if (PointInTriangle((float)x+0.5f, (float)y+0.5f, v0, v1, v2)) {
                mask.data[y*mask.width + x] = std::max(mask.data[y*mask.width + x], value);
            }
        }
    }
}

bool FaceMaskGenerator::GenerateFaceMask(const HFFaceMesh& mesh, int imageWidth, int imageHeight, FaceMask& outMask, std::string& outError) {
    if (!mesh.IsValid()) {
        outError = "Invalid mesh for mask generation";
        return false;
    }
    if (imageWidth<=0 || imageHeight<=0) {
        outError = "Invalid image dimensions for mask";
        return false;
    }
    outMask.width = imageWidth;
    outMask.height = imageHeight;
    outMask.data.assign((size_t)imageWidth*imageHeight, 0);

    // Rasterize each triangle
    for (size_t i=0; i+2 < mesh.indices.size(); i+=3) {
        int i0 = mesh.indices[i];
        int i1 = mesh.indices[i+1];
        int i2 = mesh.indices[i+2];
        if (i0<0||i0>=(int)mesh.vertices.size()||i1<0||i1>=(int)mesh.vertices.size()||i2<0||i2>=(int)mesh.vertices.size()) continue;
        HFVec2 v0(mesh.vertices[i0].x, mesh.vertices[i0].y);
        HFVec2 v1(mesh.vertices[i1].x, mesh.vertices[i1].y);
        HFVec2 v2(mesh.vertices[i2].x, mesh.vertices[i2].y);
        RasterizeTriangle(v0, v1, v2, outMask, 255);
    }

    return true;
}

std::vector<int> FaceMaskGenerator::GetFeatureLandmarkIndices(FeatureMaskType type) {
    // Mapping not proprietary, based on 68-landmark standard layout
    // This is open knowledge: 0-16 jaw, 17-21 right brow, 22-26 left brow, 27-30 nose bridge, 31-35 nose tip, 36-41 right eye, 42-47 left eye, 48-60 outer lip, 61-67 inner lip
    switch (type) {
        case FeatureMaskType::FACE:
            // All landmarks
            {
                std::vector<int> all;
                for (int i=0;i<68;++i) all.push_back(i);
                return all;
            }
        case FeatureMaskType::LIP:
            // Outer lip 48-60 + inner lip 60-67
            {
                std::vector<int> lip;
                for (int i=48;i<68;++i) lip.push_back(i);
                return lip;
            }
        case FeatureMaskType::EYE_LEFT:
            {
                std::vector<int> eye;
                for (int i=42;i<48;++i) eye.push_back(i);
                return eye;
            }
        case FeatureMaskType::EYE_RIGHT:
            {
                std::vector<int> eye;
                for (int i=36;i<42;++i) eye.push_back(i);
                return eye;
            }
        default:
            return {};
    }
}

bool FaceMaskGenerator::GenerateFeatureMask(const HFFaceData& face, FeatureMaskType type, int imageWidth, int imageHeight, FaceMask& outMask, std::string& outError) {
    if (imageWidth<=0 || imageHeight<=0) {
        outError = "Invalid image dimensions";
        return false;
    }
    outMask.width = imageWidth;
    outMask.height = imageHeight;
    outMask.data.assign((size_t)imageWidth*imageHeight, 0);

    if (type == FeatureMaskType::FACE) {
        // Use mesh if available
        if (face.mesh.IsValid()) {
            return GenerateFaceMask(face.mesh, imageWidth, imageHeight, outMask, outError);
        }
        // Fallback: bbox as mask
        int x0 = (int)std::max(0.0f, face.bboxX);
        int y0 = (int)std::max(0.0f, face.bboxY);
        int x1 = (int)std::min((float)imageWidth, face.bboxX+face.bboxW);
        int y1 = (int)std::min((float)imageHeight, face.bboxY+face.bboxH);
        for (int y=y0; y<y1; ++y) {
            for (int x=x0; x<x1; ++x) {
                outMask.data[y*imageWidth+x]=255;
            }
        }
        return true;
    } else if (type == FeatureMaskType::LIP) {
        // Lip mask from landmarks 48-67
        if (face.landmarks.size() < 68) {
            outError = "Not enough landmarks for lip mask, need 68 got "+std::to_string(face.landmarks.size());
            return false;
        }
        // Rasterize lip as polygon: outer lip 48-59, inner lip 60-67 as hole? For minimal, just rasterize triangles from lip landmarks
        // Create triangles: use fan from center
        // Outer lip: 48-59 (12 points)
        // Compute center of lip
        float cx=0, cy=0;
        for (int i=48;i<60;++i) { cx+=face.landmarks[i].x; cy+=face.landmarks[i].y; }
        cx/=12.0f; cy/=12.0f;
        HFVec2 center(cx,cy);
        for (int i=48;i<60;++i) {
            int j = (i==59)?48:i+1;
            HFVec2 v0(face.landmarks[i].x, face.landmarks[i].y);
            HFVec2 v1(face.landmarks[j].x, face.landmarks[j].y);
            RasterizeTriangle(center, v0, v1, outMask, 255);
        }
        // For inner lip, we could create hole, but for minimal lip color we want full lip including inner? Actually lip color should exclude inner mouth (teeth)
        // So we create mask for outer lip and then clear inner lip area
        FaceMask innerMask;
        innerMask.width=imageWidth;
        innerMask.height=imageHeight;
        innerMask.data.assign((size_t)imageWidth*imageHeight,0);
        float icx=0, icy=0;
        for (int i=60;i<68;++i) { icx+=face.landmarks[i].x; icy+=face.landmarks[i].y; }
        icx/=8.0f; icy/=8.0f;
        HFVec2 innerCenter(icx,icy);
        for (int i=60;i<68;++i) {
            int j = (i==67)?60:i+1;
            HFVec2 v0(face.landmarks[i].x, face.landmarks[i].y);
            HFVec2 v1(face.landmarks[j].x, face.landmarks[j].y);
            RasterizeTriangle(innerCenter, v0, v1, innerMask, 255);
        }
        // Subtract inner from outer (set inner to 0)
        for (size_t idx=0; idx<outMask.data.size(); ++idx) {
            if (innerMask.data[idx]>0) outMask.data[idx]=0;
        }
        return true;
    } else {
        outError = "FeatureMaskType not implemented for Phase 4 minimal, only FACE and LIP";
        return false;
    }
}

} // namespace huanface
