/**
 * Makeup Mask Generator — Phase 6 Full Makeup Renderer
 * REAL landmark + REAL mesh -> REAL semantic mask via polygon/triangle rasterization
 */

#include "makeup_mask.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace huanface {

// ============================================================================
// Helpers
// ============================================================================
bool MakeupMaskGenerator::PointInTriangle(float px, float py, const HFVec2& v0, const HFVec2& v1, const HFVec2& v2) {
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

bool MakeupMaskGenerator::PointInPolygon(float px, float py, const std::vector<HFVec2>& polygon) {
    // Ray casting algorithm
    if (polygon.size() < 3) return false;
    bool inside = false;
    size_t n = polygon.size();
    for (size_t i=0, j=n-1; i<n; j=i++) {
        float xi = polygon[i].x, yi = polygon[i].y;
        float xj = polygon[j].x, yj = polygon[j].y;
        bool intersect = ((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi + 1e-6f) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

void MakeupMaskGenerator::RasterizeTriangle(const HFVec2& v0, const HFVec2& v1, const HFVec2& v2, HFMakeupMask& mask, float value) {
    if (!mask.IsValid()) return;
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
            if (PointInTriangle((float)x+0.5f, (float)y+0.5f, v0, v1, v2)) {
                size_t idx = y*mask.width + x;
                mask.alpha[idx] = std::max(mask.alpha[idx], value);
            }
        }
    }
}

void MakeupMaskGenerator::RasterizePolygon(const std::vector<HFVec2>& polygon, HFMakeupMask& mask, float value) {
    if (!mask.IsValid() || polygon.size()<3) return;
    // Bounding box
    float minXf=polygon[0].x, maxXf=polygon[0].x, minYf=polygon[0].y, maxYf=polygon[0].y;
    for (auto& p: polygon) {
        minXf = std::min(minXf, p.x);
        maxXf = std::max(maxXf, p.x);
        minYf = std::min(minYf, p.y);
        maxYf = std::max(maxYf, p.y);
    }
    int minX = std::max(0, (int)std::floor(minXf));
    int maxX = std::min(mask.width-1, (int)std::ceil(maxXf));
    int minY = std::max(0, (int)std::floor(minYf));
    int maxY = std::min(mask.height-1, (int)std::ceil(maxYf));
    for (int y=minY; y<=maxY; ++y) {
        for (int x=minX; x<=maxX; ++x) {
            if (PointInPolygon((float)x+0.5f, (float)y+0.5f, polygon)) {
                size_t idx = y*mask.width + x;
                mask.alpha[idx] = std::max(mask.alpha[idx], value);
            }
        }
    }
}

std::vector<HFVec2> MakeupMaskGenerator::GetLandmarkPolygon(const HFFaceData& face, const std::vector<int>& indices) {
    std::vector<HFVec2> poly;
    poly.reserve(indices.size());
    for (int idx: indices) {
        if (idx>=0 && idx < (int)face.landmarks.size()) {
            poly.push_back(face.landmarks[idx]);
        }
    }
    return poly;
}

std::vector<int> MakeupMaskGenerator::GetLandmarkIndicesForMask(MakeupMaskType type) {
    // 68-point standard mapping
    switch(type) {
        case MakeupMaskType::Face:
            { std::vector<int> all; for(int i=0;i<68;++i) all.push_back(i); return all; }
        case MakeupMaskType::Lip:
            { std::vector<int> lip; for(int i=48;i<68;++i) lip.push_back(i); return lip; }
        case MakeupMaskType::UpperLip:
            { std::vector<int> upper = {48,49,50,51,52,53,54,60,61,62,63,64}; return upper; }
        case MakeupMaskType::LowerLip:
            { std::vector<int> lower = {54,55,56,57,58,59,48,60,67,66,65,64}; return lower; }
        case MakeupMaskType::LeftEye: // left eye from viewer's perspective? Actually 42-47 left eye
            { std::vector<int> eye; for(int i=42;i<48;++i) eye.push_back(i); return eye; }
        case MakeupMaskType::RightEye:
            { std::vector<int> eye; for(int i=36;i<42;++i) eye.push_back(i); return eye; }
        case MakeupMaskType::LeftEyelid:
            { std::vector<int> eyelid = {42,43,44,45,46,47}; return eyelid; }
        case MakeupMaskType::RightEyelid:
            { std::vector<int> eyelid = {36,37,38,39,40,41}; return eyelid; }
        case MakeupMaskType::LeftEyebrow:
            { std::vector<int> brow; for(int i=22;i<27;++i) brow.push_back(i); return brow; }
        case MakeupMaskType::RightEyebrow:
            { std::vector<int> brow; for(int i=17;i<22;++i) brow.push_back(i); return brow; }
        case MakeupMaskType::LeftCheek:
            { return {0,1,2,31,48}; } // approximate cheek via jaw + nose + lip
        case MakeupMaskType::RightCheek:
            { return {14,15,16,35,54}; }
        case MakeupMaskType::Nose:
            { std::vector<int> nose; for(int i=27;i<36;++i) nose.push_back(i); return nose; }
        default:
            return {};
    }
}

// ============================================================================
// Mask Operations — feather, blur, opacity, dilate, erode
// ============================================================================
void MakeupMaskGenerator::ApplyFeather(HFMakeupMask& mask, float featherRadius) {
    if (featherRadius <= 0.0f || !mask.IsValid()) return;
    mask.feather = featherRadius;
    // Simple box blur feathering — for CPU reference
    int radius = (int)std::ceil(featherRadius);
    if (radius <=0) return;
    std::vector<float> tmp = mask.alpha;
    int w = mask.width, h = mask.height;
    // Horizontal blur
    for (int y=0; y<h; ++y) {
        for (int x=0; x<w; ++x) {
            float sum=0; int count=0;
            for (int dx=-radius; dx<=radius; ++dx) {
                int nx = x+dx;
                if (nx<0||nx>=w) continue;
                sum += tmp[y*w+nx];
                count++;
            }
            if (count>0) mask.alpha[y*w+x] = sum / count;
        }
    }
    tmp = mask.alpha;
    // Vertical blur
    for (int y=0; y<h; ++y) {
        for (int x=0; x<w; ++x) {
            float sum=0; int count=0;
            for (int dy=-radius; dy<=radius; ++dy) {
                int ny = y+dy;
                if (ny<0||ny>=h) continue;
                sum += tmp[ny*w+x];
                count++;
            }
            if (count>0) mask.alpha[y*w+x] = sum / count;
        }
    }
}

void MakeupMaskGenerator::ApplyBlur(HFMakeupMask& mask, float blurRadius) {
    ApplyFeather(mask, blurRadius);
    mask.blurRadius = blurRadius;
}

void MakeupMaskGenerator::ApplyOpacity(HFMakeupMask& mask, float opacity) {
    if (!mask.IsValid()) return;
    opacity = std::max(0.0f, std::min(1.0f, opacity));
    mask.opacity = opacity;
    for (float& a : mask.alpha) {
        a = std::max(0.0f, std::min(1.0f, a * opacity));
    }
}

void MakeupMaskGenerator::DilateMask(HFMakeupMask& mask, float radius) {
    if (radius <=0 || !mask.IsValid()) return;
    int r = (int)std::ceil(radius);
    std::vector<float> tmp = mask.alpha;
    int w=mask.width, h=mask.height;
    for (int y=0; y<h; ++y) {
        for (int x=0; x<w; ++x) {
            float maxA = 0;
            for (int dy=-r; dy<=r; ++dy) {
                for (int dx=-r; dx<=r; ++dx) {
                    int nx=x+dx, ny=y+dy;
                    if (nx<0||nx>=w||ny<0||ny>=h) continue;
                    maxA = std::max(maxA, tmp[ny*w+nx]);
                }
            }
            mask.alpha[y*w+x] = maxA;
        }
    }
}

void MakeupMaskGenerator::ErodeMask(HFMakeupMask& mask, float radius) {
    if (radius <=0 || !mask.IsValid()) return;
    int r = (int)std::ceil(radius);
    std::vector<float> tmp = mask.alpha;
    int w=mask.width, h=mask.height;
    for (int y=0; y<h; ++y) {
        for (int x=0; x<w; ++x) {
            float minA = 1.0f;
            for (int dy=-r; dy<=r; ++dy) {
                for (int dx=-r; dx<=r; ++dx) {
                    int nx=x+dx, ny=y+dy;
                    if (nx<0||nx>=w||ny<0||ny>=h) continue;
                    minA = std::min(minA, tmp[ny*w+nx]);
                }
            }
            mask.alpha[y*w+x] = minA;
        }
    }
}

// ============================================================================
// Validation
// ============================================================================
bool MakeupMaskGenerator::ValidateMask(const HFMakeupMask& mask, std::string& outError) {
    if (!mask.IsValid()) { outError="Invalid mask dimensions"; return false; }
    if (!mask.HasFinite()) { outError="Mask has NaN/Inf"; return false; }
    float minA = mask.MinAlpha();
    float maxA = mask.MaxAlpha();
    if (minA < -0.001f) { outError="Mask minAlpha <0: "+std::to_string(minA); return false; }
    if (maxA > 1.001f) { outError="Mask maxAlpha >1: "+std::to_string(maxA); return false; }
    if (!mask.HasNonZero()) { outError="Mask has zero coverage"; return false; }
    // Coverage should not be too large (should be inside face, not full image)
    float cov = mask.Coverage();
    if (cov > 0.8f) {
        // Face mask can be up to 30% typically, but allow larger for foundation
        // For lip/eye it should be small
        // We'll just warn, not fail for face
    }
    return true;
}

bool MakeupMaskGenerator::ValidateMaskFollowsLandmarks(const HFFaceData& face1, const HFFaceData& face2,
                                                      MakeupMaskType type, int w, int h) {
    MakeupMaskGenerator gen;
    HFMakeupMask m1, m2;
    std::string err;
    if (!gen.GenerateMask(face1, type, w, h, m1, err)) return false;
    if (!gen.GenerateMask(face2, type, w, h, m2, err)) return false;
    // Masks should differ when landmarks move
    // Compute centroid difference
    float cx1=0, cy1=0, sum1=0;
    float cx2=0, cy2=0, sum2=0;
    for (int y=0;y<h;++y) for(int x=0;x<w;++x){
        float a1=m1.alpha[y*w+x];
        float a2=m2.alpha[y*w+x];
        cx1+=x*a1; cy1+=y*a1; sum1+=a1;
        cx2+=x*a2; cy2+=y*a2; sum2+=a2;
    }
    if (sum1>0) { cx1/=sum1; cy1/=sum1; }
    if (sum2>0) { cx2/=sum2; cy2/=sum2; }
    float dist = std::hypot(cx1-cx2, cy1-cy2);
    // If landmarks moved, mask centroid should move at least 1 pixel
    return dist > 0.5f;
}

// ============================================================================
// Specific Mask Generators — REAL landmark + mesh
// ============================================================================
bool MakeupMaskGenerator::GenerateFaceMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err) {
    if (w<=0||h<=0) { err="Invalid dimensions"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign((size_t)w*h, 0.0f);
    out.feather=0; out.blurRadius=0; out.opacity=1.0f;

    if (face.mesh.IsValid()) {
        // Use mesh triangles
        for (size_t i=0;i+2<face.mesh.indices.size();i+=3) {
            int i0=face.mesh.indices[i];
            int i1=face.mesh.indices[i+1];
            int i2=face.mesh.indices[i+2];
            if (i0<0||i0>=(int)face.mesh.vertices.size()||i1<0||i1>=(int)face.mesh.vertices.size()||i2<0||i2>=(int)face.mesh.vertices.size()) continue;
            HFVec2 v0(face.mesh.vertices[i0].x, face.mesh.vertices[i0].y);
            HFVec2 v1(face.mesh.vertices[i1].x, face.mesh.vertices[i1].y);
            HFVec2 v2(face.mesh.vertices[i2].x, face.mesh.vertices[i2].y);
            RasterizeTriangle(v0,v1,v2,out,1.0f);
        }
    } else if (face.landmarks.size()>=17) {
        // Fallback: use jaw landmarks 0-16 + forehead approximation
        std::vector<HFVec2> poly;
        for(int i=0;i<17;++i) poly.push_back(face.landmarks[i]);
        // Add forehead points: top of face via brow
        if (face.landmarks.size()>=27) {
            // Approximate forehead above brows
            HFVec2 leftBrow = face.landmarks[19];
            HFVec2 rightBrow = face.landmarks[24];
            float browY = (leftBrow.y + rightBrow.y)*0.5f;
            float faceTopY = face.bboxY + face.bboxH*0.1f;
            // Add points above brows
            poly.push_back(HFVec2(face.landmarks[26].x, faceTopY));
            poly.push_back(HFVec2(face.landmarks[17].x, faceTopY));
        }
        RasterizePolygon(poly, out, 1.0f);
    } else {
        // Last fallback: bbox
        int x0 = std::max(0, (int)face.bboxX);
        int y0 = std::max(0, (int)face.bboxY);
        int x1 = std::min(w, (int)(face.bboxX+face.bboxW));
        int y1 = std::min(h, (int)(face.bboxY+face.bboxH));
        for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) out.alpha[y*w+x]=1.0f;
    }
    // Apply soft feather for face mask
    ApplyFeather(out, 2.0f);
    return true;
}

bool MakeupMaskGenerator::GenerateLipMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err, bool upperOnly, bool lowerOnly) {
    if (w<=0||h<=0) { err="Invalid dims"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign((size_t)w*h,0.0f);

    if (face.landmarks.size()<68) { err="Need 68 landmarks for lip"; return false; }

    if (upperOnly) {
        std::vector<int> idx = GetLandmarkIndicesForMask(MakeupMaskType::UpperLip);
        auto poly = GetLandmarkPolygon(face, idx);
        RasterizePolygon(poly, out, 1.0f);
    } else if (lowerOnly) {
        std::vector<int> idx = GetLandmarkIndicesForMask(MakeupMaskType::LowerLip);
        auto poly = GetLandmarkPolygon(face, idx);
        RasterizePolygon(poly, out, 1.0f);
    } else {
        // Outer lip polygon
        std::vector<int> outerIdx;
        for(int i=48;i<60;++i) outerIdx.push_back(i);
        auto outerPoly = GetLandmarkPolygon(face, outerIdx);
        // Use fan triangulation for outer lip
        float cx=0, cy=0;
        for(auto& p: outerPoly){ cx+=p.x; cy+=p.y; }
        cx/=outerPoly.size(); cy/=outerPoly.size();
        HFVec2 center(cx,cy);
        for(size_t i=0;i<outerPoly.size();++i){
            size_t j=(i+1)%outerPoly.size();
            RasterizeTriangle(center, outerPoly[i], outerPoly[j], out, 1.0f);
        }
        // Subtract inner lip (hole for teeth)
        HFMakeupMask inner;
        inner.width=w; inner.height=h;
        inner.alpha.assign((size_t)w*h,0.0f);
        std::vector<int> innerIdx;
        for(int i=60;i<68;++i) innerIdx.push_back(i);
        auto innerPoly = GetLandmarkPolygon(face, innerIdx);
        float icx=0, icy=0;
        for(auto& p: innerPoly){ icx+=p.x; icy+=p.y; }
        icx/=innerPoly.size(); icy/=innerPoly.size();
        HFVec2 innerCenter(icx,icy);
        for(size_t i=0;i<innerPoly.size();++i){
            size_t j=(i+1)%innerPoly.size();
            RasterizeTriangle(innerCenter, innerPoly[i], innerPoly[j], inner, 1.0f);
        }
        for(size_t k=0;k<out.alpha.size();++k){
            if (inner.alpha[k]>0.5f) out.alpha[k]=0.0f;
        }
    }
    ApplyFeather(out, 1.0f);
    return true;
}

bool MakeupMaskGenerator::GenerateEyeMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err, bool left, bool eyelidOnly) {
    if (w<=0||h<=0) { err="Invalid dims"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign((size_t)w*h,0.0f);
    if (face.landmarks.size()<68) { err="Need 68 landmarks"; return false; }

    MakeupMaskType type = left ? (eyelidOnly? MakeupMaskType::LeftEyelid : MakeupMaskType::LeftEye) : (eyelidOnly? MakeupMaskType::RightEyelid : MakeupMaskType::RightEye);
    auto indices = GetLandmarkIndicesForMask(type);
    auto poly = GetLandmarkPolygon(face, indices);
    if (poly.size()<3) { err="Not enough points for eye"; return false; }

    // Fan triangulation
    float cx=0, cy=0;
    for(auto& p: poly){ cx+=p.x; cy+=p.y; }
    cx/=poly.size(); cy/=poly.size();
    HFVec2 center(cx,cy);
    for(size_t i=0;i<poly.size();++i){
        size_t j=(i+1)%poly.size();
        RasterizeTriangle(center, poly[i], poly[j], out, 1.0f);
    }

    if (eyelidOnly) {
        // For eyelid, expand slightly above eye
        DilateMask(out, 2.0f);
        // Create eyelid region: upper part of eye + above
        // Approximate by shifting polygon up
        std::vector<HFVec2> upperPoly = poly;
        // Find topmost points and expand
        // For simplicity, just feather
    }

    ApplyFeather(out, 1.0f);
    return true;
}

bool MakeupMaskGenerator::GenerateEyebrowMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err, bool left) {
    if (w<=0||h<=0) { err="Invalid dims"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign((size_t)w*h,0.0f);
    if (face.landmarks.size()<27) { err="Need brows"; return false; }

    auto indices = GetLandmarkIndicesForMask(left ? MakeupMaskType::LeftEyebrow : MakeupMaskType::RightEyebrow);
    auto poly = GetLandmarkPolygon(face, indices);
    if (poly.size()<3) { err="Not enough brow points"; return false; }

    // Eyebrow as thickened polygon: expand bounding box slightly
    // Create polygon with extra thickness
    // For brow, we want to cover brow area, not just line
    // Dilate by creating expanded polygon
    // Simple: rasterize polygon then dilate
    // First, create closed polygon from brow points (5 points) -> make rectangle-like
    // Brow points are along arch, we need to create thick region
    // Approximate by adding points below brow
    std::vector<HFVec2> thickPoly = poly;
    // Add points below to make thick
    for (int i=(int)poly.size()-1; i>=0; --i) {
        HFVec2 p = poly[i];
        p.y += 8.0f; // thickness 8px below
        thickPoly.push_back(p);
    }
    RasterizePolygon(thickPoly, out, 1.0f);
    DilateMask(out, 1.0f);
    ApplyFeather(out, 1.5f);
    return true;
}

bool MakeupMaskGenerator::GenerateCheekMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err, bool left) {
    if (w<=0||h<=0) { err="Invalid dims"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign((size_t)w*h,0.0f);
    if (face.landmarks.size()<68) { err="Need landmarks"; return false; }

    // Cheek position: based on nose and eye and mouth landmarks, not absolute
    // Left cheek: between left eye outer, nose, mouth left corner, jaw
    // Right cheek: symmetric
    HFVec2 eyeOuter, noseSide, mouthCorner, jawPoint;
    if (left) {
        eyeOuter = face.landmarks[45]; // left eye outer? Actually 45 is left eye inner? Let's use 47 leftmost
        eyeOuter = face.landmarks[42]; // left eye outer corner
        noseSide = face.landmarks[31]; // nose left
        mouthCorner = face.landmarks[48]; // mouth left
        jawPoint = face.landmarks[2]; // jaw left
    } else {
        eyeOuter = face.landmarks[39]; // right eye outer
        noseSide = face.landmarks[35]; // nose right
        mouthCorner = face.landmarks[54]; // mouth right
        jawPoint = face.landmarks[14]; // jaw right
    }

    // Cheek center: average of those points with offset
    float cx = (eyeOuter.x + noseSide.x + mouthCorner.x + jawPoint.x)*0.25f;
    float cy = (eyeOuter.y + noseSide.y + mouthCorner.y + jawPoint.y)*0.25f;
    // Adjust: cheek is below eye, to side of nose
    cy += face.bboxH * 0.05f;
    if (left) cx -= face.bboxW * 0.05f;
    else cx += face.bboxW * 0.05f;

    // Create ellipse-like mask via rasterizing circle
    float radiusX = face.bboxW * 0.15f;
    float radiusY = face.bboxH * 0.12f;
    int minX = std::max(0, (int)(cx - radiusX));
    int maxX = std::min(w-1, (int)(cx + radiusX));
    int minY = std::max(0, (int)(cy - radiusY));
    int maxY = std::min(h-1, (int)(cy + radiusY));
    for (int y=minY; y<=maxY; ++y) {
        for (int x=minX; x<=maxX; ++x) {
            float dx = (x - cx)/radiusX;
            float dy = (y - cy)/radiusY;
            float dist = dx*dx + dy*dy;
            if (dist <= 1.0f) {
                float alpha = 1.0f - dist; // soft edge
                alpha = std::max(0.0f, std::min(1.0f, alpha));
                out.alpha[y*w+x] = std::max(out.alpha[y*w+x], alpha);
            }
        }
    }
    ApplyFeather(out, 3.0f);
    return true;
}

bool MakeupMaskGenerator::GenerateNoseMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err) {
    if (w<=0||h<=0) { err="Invalid dims"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign((size_t)w*h,0.0f);
    if (face.landmarks.size()<36) { err="Need nose landmarks"; return false; }

    auto indices = GetLandmarkIndicesForMask(MakeupMaskType::Nose);
    auto poly = GetLandmarkPolygon(face, indices);
    if (poly.empty()) { err="No nose poly"; return false; }
    // Nose as polygon from bridge to tip
    RasterizePolygon(poly, out, 1.0f);
    DilateMask(out, 1.0f);
    ApplyFeather(out, 1.5f);
    return true;
}

bool MakeupMaskGenerator::GenerateMask(const HFFaceData& face, MakeupMaskType type, int w, int h, HFMakeupMask& out, std::string& err) {
    switch(type) {
        case MakeupMaskType::Face: return GenerateFaceMask(face,w,h,out,err);
        case MakeupMaskType::Lip: return GenerateLipMask(face,w,h,out,err,false,false);
        case MakeupMaskType::UpperLip: return GenerateLipMask(face,w,h,out,err,true,false);
        case MakeupMaskType::LowerLip: return GenerateLipMask(face,w,h,out,err,false,true);
        case MakeupMaskType::LeftEye: return GenerateEyeMask(face,w,h,out,err,true,false);
        case MakeupMaskType::RightEye: return GenerateEyeMask(face,w,h,out,err,false,false);
        case MakeupMaskType::LeftEyelid: return GenerateEyeMask(face,w,h,out,err,true,true);
        case MakeupMaskType::RightEyelid: return GenerateEyeMask(face,w,h,out,err,false,true);
        case MakeupMaskType::LeftEyebrow: return GenerateEyebrowMask(face,w,h,out,err,true);
        case MakeupMaskType::RightEyebrow: return GenerateEyebrowMask(face,w,h,out,err,false);
        case MakeupMaskType::LeftCheek: return GenerateCheekMask(face,w,h,out,err,true);
        case MakeupMaskType::RightCheek: return GenerateCheekMask(face,w,h,out,err,false);
        case MakeupMaskType::Nose: return GenerateNoseMask(face,w,h,out,err);
        default: err="Unknown mask type"; return false;
    }
}

bool MakeupMaskGenerator::GenerateAllMasks(const HFFaceData& face, int w, int h, std::map<MakeupMaskType, HFMakeupMask>& outMasks, std::string& err) {
    outMasks.clear();
    for (int i=0;i<(int)MakeupMaskType::Count;++i) {
        MakeupMaskType type = (MakeupMaskType)i;
        HFMakeupMask mask;
        std::string e;
        if (GenerateMask(face, type, w, h, mask, e)) {
            outMasks[type]=mask;
        } else {
            // For required masks, fail? But we allow partial
            // Only fail if Face mask fails
            if (type==MakeupMaskType::Face) { err=e; return false; }
        }
    }
    return true;
}

} // namespace huanface
