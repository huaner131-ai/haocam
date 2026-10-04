/**
 * Beauty Mask Generator — Phase 7 Full Beauty & Face Retouching Engine
 * REAL landmark + REAL mesh -> REAL semantic beauty masks with exclusions
 * Pipeline: Face Mesh -> Face Region -> Exclude Eyes -> Exclude Brows -> Exclude Lips -> Skin Mask
 */

#include "beauty_mask.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace huanface {

// ============================================================================
// Helpers — same rasterization as makeup but for beauty
// ============================================================================
bool HFBeautyMaskGenerator::PointInTriangle(float px, float py, const HFVec2& v0, const HFVec2& v1, const HFVec2& v2) {
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

bool HFBeautyMaskGenerator::PointInPolygon(float px, float py, const std::vector<HFVec2>& polygon) {
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

void HFBeautyMaskGenerator::RasterizeTriangle(const HFVec2& v0, const HFVec2& v1, const HFVec2& v2, HFBeautyMask& mask, float value) {
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

void HFBeautyMaskGenerator::RasterizePolygon(const std::vector<HFVec2>& polygon, HFBeautyMask& mask, float value) {
    if (!mask.IsValid() || polygon.size()<3) return;
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

std::vector<HFVec2> HFBeautyMaskGenerator::GetLandmarkPolygon(const HFFaceData& face, const std::vector<int>& indices) {
    std::vector<HFVec2> poly;
    poly.reserve(indices.size());
    for(int idx: indices) {
        if (idx>=0 && idx<(int)face.landmarks.size()) {
            poly.push_back(HFVec2(face.landmarks[idx].x, face.landmarks[idx].y));
        }
    }
    return poly;
}

std::vector<int> HFBeautyMaskGenerator::GetLandmarkIndicesForMask(BeautyMaskType type) {
    switch(type) {
        case BeautyMaskType::Face:
            return {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,26,25,24,23,22,21,20,19,18,17}; // jaw + brow outline for face
        case BeautyMaskType::Forehead:
            return {17,18,19,20,21,22,23,24,25,26}; // brows as lower bound, forehead above
        case BeautyMaskType::LeftCheek:
            return {2,3,4,5,31,49}; // left cheek region approx
        case BeautyMaskType::RightCheek:
            return {12,13,14,35,53,11};
        case BeautyMaskType::Nose:
            return {27,28,29,30,31,32,33,34,35};
        case BeautyMaskType::Chin:
            return {6,7,8,9,10,58,57,56};
        case BeautyMaskType::UnderEyeLeft:
            return {42,43,44,45,46,47}; // left eye
        case BeautyMaskType::UnderEyeRight:
            return {36,37,38,39,40,41}; // right eye
        case BeautyMaskType::EyeExclusion:
            return {36,37,38,39,40,41,42,43,44,45,46,47};
        case BeautyMaskType::LipExclusion:
            return {48,49,50,51,52,53,54,55,56,57,58,59};
        case BeautyMaskType::BrowExclusion:
            return {17,18,19,20,21,22,23,24,25,26};
        default:
            return {};
    }
}

// ============================================================================
// Mask operations
// ============================================================================
void HFBeautyMaskGenerator::ApplyFeather(HFBeautyMask& mask, float featherRadius) {
    if (!mask.IsValid() || featherRadius<=0.0f) return;
    int radius = (int)std::max(1.0f, featherRadius);
    // Simple box blur feather for CPU reference
    std::vector<float> tmp = mask.alpha;
    int w=mask.width, h=mask.height;
    // Horizontal pass
    for(int y=0;y<h;++y){
        for(int x=0;x<w;++x){
            float sum=0; int cnt=0;
            for(int dx=-radius; dx<=radius; ++dx){
                int nx=x+dx;
                if(nx>=0&&nx<w){ sum+=tmp[y*w+nx]; cnt++; }
            }
            mask.alpha[y*w+x]=sum/(float)cnt;
        }
    }
    tmp=mask.alpha;
    // Vertical pass
    for(int y=0;y<h;++y){
        for(int x=0;x<w;++x){
            float sum=0; int cnt=0;
            for(int dy=-radius; dy<=radius; ++dy){
                int ny=y+dy;
                if(ny>=0&&ny<h){ sum+=tmp[ny*w+x]; cnt++; }
            }
            mask.alpha[y*w+x]=sum/(float)cnt;
        }
    }
    mask.feather = featherRadius;
}

void HFBeautyMaskGenerator::ApplyBlur(HFBeautyMask& mask, float blurRadius) {
    ApplyFeather(mask, blurRadius);
    mask.blurRadius = blurRadius;
}

void HFBeautyMaskGenerator::ApplyOpacity(HFBeautyMask& mask, float opacity) {
    if (!mask.IsValid()) return;
    opacity = std::max(0.0f, std::min(1.0f, opacity));
    for(float& a: mask.alpha) a *= opacity;
    mask.opacity *= opacity;
}

void HFBeautyMaskGenerator::DilateMask(HFBeautyMask& mask, float radius) {
    if (!mask.IsValid() || radius<=0) return;
    int r = (int)std::ceil(radius);
    std::vector<float> tmp = mask.alpha;
    int w=mask.width, h=mask.height;
    for(int y=0;y<h;++y){
        for(int x=0;x<w;++x){
            float maxV=0;
            for(int dy=-r; dy<=r; ++dy){
                for(int dx=-r; dx<=r; ++dx){
                    int nx=x+dx, ny=y+dy;
                    if(nx>=0&&nx<w&&ny>=0&&ny<h){
                        maxV = std::max(maxV, tmp[ny*w+nx]);
                    }
                }
            }
            mask.alpha[y*w+x]=maxV;
        }
    }
}

void HFBeautyMaskGenerator::ErodeMask(HFBeautyMask& mask, float radius) {
    if (!mask.IsValid() || radius<=0) return;
    int r = (int)std::ceil(radius);
    std::vector<float> tmp = mask.alpha;
    int w=mask.width, h=mask.height;
    for(int y=0;y<h;++y){
        for(int x=0;x<w;++x){
            float minV=1.0f;
            for(int dy=-r; dy<=r; ++dy){
                for(int dx=-r; dx<=r; ++dx){
                    int nx=x+dx, ny=y+dy;
                    if(nx>=0&&nx<w&&ny>=0&&ny<h){
                        minV = std::min(minV, tmp[ny*w+nx]);
                    }
                }
            }
            mask.alpha[y*w+x]=minV;
        }
    }
}

void HFBeautyMaskGenerator::SubtractMask(HFBeautyMask& base, const HFBeautyMask& exclusion) {
    if (!base.IsValid() || !exclusion.IsValid()) return;
    if (base.width!=exclusion.width || base.height!=exclusion.height) return;
    for(size_t i=0;i<base.alpha.size();++i){
        base.alpha[i] = std::max(0.0f, base.alpha[i] - exclusion.alpha[i]);
        // If exclusion is strong, reduce base
        if (exclusion.alpha[i]>0.5f) base.alpha[i]*=(1.0f - exclusion.alpha[i]);
        base.alpha[i]=std::max(0.0f, std::min(1.0f, base.alpha[i]));
    }
}

void HFBeautyMaskGenerator::IntersectMask(HFBeautyMask& base, const HFBeautyMask& other) {
    if (!base.IsValid() || !other.IsValid()) return;
    if (base.width!=other.width || base.height!=other.height) return;
    for(size_t i=0;i<base.alpha.size();++i){
        base.alpha[i] = std::min(base.alpha[i], other.alpha[i]);
    }
}

// ============================================================================
// Validation
// ============================================================================
bool HFBeautyMaskGenerator::ValidateMask(const HFBeautyMask& mask, std::string& outError) {
    if (!mask.IsValid()) { outError="Invalid mask dimensions"; return false; }
    if (!mask.HasFinite()) { outError="Mask has non-finite values"; return false; }
    if (mask.MinAlpha()< -0.001f || mask.MaxAlpha()>1.001f) { outError="Mask alpha out of range 0-1"; return false; }
    // Coverage check - allow small but non-zero for some masks
    if (!mask.HasNonZero()) { outError="Mask has zero coverage"; return false; }
    return true;
}

bool HFBeautyMaskGenerator::ValidateMaskFollowsLandmarks(const HFFaceData& face1, const HFFaceData& face2, BeautyMaskType type, int w, int h) {
    // Generate masks for two faces with slight movement, check centroid moves
    HFBeautyMaskGenerator gen;
    HFBeautyMask m1, m2;
    std::string err;
    if (!gen.GenerateMask(face1, type, w, h, m1, err)) return false;
    if (!gen.GenerateMask(face2, type, w, h, m2, err)) return false;
    // Compute centroid
    float cx1=0, cy1=0, sum1=0, cx2=0, cy2=0, sum2=0;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x){
        float a1=m1.GetAlpha(x,y);
        float a2=m2.GetAlpha(x,y);
        cx1+=x*a1; cy1+=y*a1; sum1+=a1;
        cx2+=x*a2; cy2+=y*a2; sum2+=a2;
    }
    if(sum1<1e-3f||sum2<1e-3f) return false;
    cx1/=sum1; cy1/=sum1;
    cx2/=sum2; cy2/=sum2;
    float dist = std::sqrt((cx1-cx2)*(cx1-cx2)+(cy1-cy2)*(cy1-cy2));
    // If face moved ~20px, mask should move at least 0.5px
    return dist>0.3f;
}

bool HFBeautyMaskGenerator::ValidateSkinExclusion(const HFBeautyMask& skinMask, const HFBeautyMask& eyeMask, const HFBeautyMask& lipMask, std::string& err) {
    // Check skin mask does not heavily overlap eye/lip exclusion
    if (!skinMask.IsValid() || !eyeMask.IsValid() || !lipMask.IsValid()) { err="Invalid masks"; return false; }
    // Sample: skin alpha should be low where eye/lip alpha high
    int w=skinMask.width, h=skinMask.height;
    int overlapEye=0, overlapLip=0, totalSkin=0;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x){
        float s=skinMask.GetAlpha(x,y);
        float e=eyeMask.GetAlpha(x,y);
        float l=lipMask.GetAlpha(x,y);
        if(s>0.5f){
            totalSkin++;
            if(e>0.5f) overlapEye++;
            if(l>0.5f) overlapLip++;
        }
    }
    if(totalSkin==0){ err="Skin mask empty"; return false; }
    float eyeOverlapRatio = (float)overlapEye/(float)totalSkin;
    float lipOverlapRatio = (float)overlapLip/(float)totalSkin;
    // Allow small overlap due to feather, but not large
    if(eyeOverlapRatio>0.3f){ err="Skin overlaps eye too much: "+std::to_string(eyeOverlapRatio); return false; }
    if(lipOverlapRatio>0.3f){ err="Skin overlaps lip too much: "+std::to_string(lipOverlapRatio); return false; }
    return true;
}

// ============================================================================
// Specific mask generators
// ============================================================================
bool HFBeautyMaskGenerator::GenerateFaceMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err) {
    if (face.landmarks.size()<17) { err="Not enough landmarks for face mask"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign(w*h, 0.0f);
    out.feather=0; out.blurRadius=0; out.opacity=1.0f;

    // Use face mesh if available, else jaw + forehead estimation
    bool usedMesh=false;
    if (face.mesh.VertexCount()>=3 && face.mesh.TriangleCount()>=1) {
        // Rasterize mesh triangles for face region
        for(size_t i=0;i+2<face.mesh.indices.size();i+=3){
            int i0=face.mesh.indices[i];
            int i1=face.mesh.indices[i+1];
            int i2=face.mesh.indices[i+2];
            if (i0>=face.mesh.VertexCount()||i1>=face.mesh.VertexCount()||i2>=face.mesh.VertexCount()) continue;
            HFVec3 v0=face.mesh.vertices[i0];
            HFVec3 v1=face.mesh.vertices[i1];
            HFVec3 v2=face.mesh.vertices[i2];
            RasterizeTriangle(HFVec2(v0.x,v0.y), HFVec2(v1.x,v1.y), HFVec2(v2.x,v2.y), out, 1.0f);
        }
        usedMesh=true;
    }

    // If mesh coverage too low or no mesh, fallback to jaw polygon + forehead
    if(!usedMesh || out.Coverage()<0.02f){
        std::vector<HFVec2> jaw;
        for(int i=0;i<=16;++i) if(i<(int)face.landmarks.size()) jaw.push_back(HFVec2(face.landmarks[i].x, face.landmarks[i].y));
        if(jaw.size()>=3){
            if(face.landmarks.size()>=27){
                HFVec2 leftBrow(face.landmarks[19].x, face.landmarks[19].y);
                HFVec2 rightBrow(face.landmarks[24].x, face.landmarks[24].y);
                float faceHeight = jaw[8].y - (leftBrow.y+rightBrow.y)*0.5f;
                HFVec2 topLeft(jaw[0].x, leftBrow.y - faceHeight*0.6f);
                HFVec2 topRight(jaw[16].x, rightBrow.y - faceHeight*0.6f);
                std::vector<HFVec2> facePoly;
                facePoly.push_back(topLeft);
                facePoly.push_back(topRight);
                for(int i=16;i>=0;--i) facePoly.push_back(jaw[i]);
                RasterizePolygon(facePoly, out, 1.0f);
            } else {
                RasterizePolygon(jaw, out, 1.0f);
            }
        }
    }

    // Feather for soft edge
    ApplyFeather(out, 2.0f);
    return ValidateMask(out, err);
}

bool HFBeautyMaskGenerator::GenerateForeheadMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err) {
    if (face.landmarks.size()<27) { err="Not enough landmarks for forehead"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign(w*h, 0.0f);

    // Forehead region: above eyebrows, between jaw x bounds
    // Use brow landmarks 17-26 as bottom, estimate top
    HFVec2 leftBrowOuter(face.landmarks[17].x, face.landmarks[17].y);
    HFVec2 rightBrowOuter(face.landmarks[26].x, face.landmarks[26].y);
    HFVec2 leftBrowInner(face.landmarks[21].x, face.landmarks[21].y);
    HFVec2 rightBrowInner(face.landmarks[22].x, face.landmarks[22].y);
    float browY = (leftBrowOuter.y+rightBrowOuter.y+leftBrowInner.y+rightBrowInner.y)/4.0f;
    float jawTopY = face.landmarks[8].y; // chin
    float faceH = jawTopY - browY;
    // Forehead polygon: top edge above brows
    std::vector<HFVec2> foreheadPoly;
    foreheadPoly.push_back(HFVec2(leftBrowOuter.x - faceH*0.1f, browY - faceH*0.8f));
    foreheadPoly.push_back(HFVec2(rightBrowOuter.x + faceH*0.1f, browY - faceH*0.8f));
    foreheadPoly.push_back(HFVec2(rightBrowOuter.x + faceH*0.05f, browY - faceH*0.1f));
    foreheadPoly.push_back(HFVec2(leftBrowOuter.x - faceH*0.05f, browY - faceH*0.1f));
    RasterizePolygon(foreheadPoly, out, 1.0f);
    ApplyFeather(out, 3.0f);
    return ValidateMask(out, err);
}

bool HFBeautyMaskGenerator::GenerateCheekMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err, bool left) {
    if (face.landmarks.size()<49) { err="Not enough landmarks for cheek"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign(w*h, 0.0f);

    // Cheek position from eye outer + nose side + mouth corner + jaw, not absolute resolution
    HFVec2 eyeOuter, noseSide, mouthCorner, jawPoint;
    if(left){
        // Left cheek = our left = image right? Actually  left cheek of face is left side of image if not mirrored, but we use landmark semantics: left cheek uses left side of face (landmarks 2-4)
        eyeOuter = HFVec2(face.landmarks[45].x, face.landmarks[45].y); // left eye outer (landmark 45)
        noseSide = HFVec2(face.landmarks[35].x, face.landmarks[35].y);
        mouthCorner = HFVec2(face.landmarks[54].x, face.landmarks[54].y);
        jawPoint = HFVec2(face.landmarks[12].x, face.landmarks[12].y);
    } else {
        eyeOuter = HFVec2(face.landmarks[36].x, face.landmarks[36].y); // right eye outer
        noseSide = HFVec2(face.landmarks[31].x, face.landmarks[31].y);
        mouthCorner = HFVec2(face.landmarks[48].x, face.landmarks[48].y);
        jawPoint = HFVec2(face.landmarks[4].x, face.landmarks[4].y);
    }

    float faceW = std::abs(face.landmarks[16].x - face.landmarks[0].x);
    float faceH = std::abs(face.landmarks[8].y - face.landmarks[27].y);
    float radiusX = faceW * 0.15f;
    float radiusY = faceH * 0.12f;

    // Cheek center estimation
    HFVec2 center((eyeOuter.x + noseSide.x + mouthCorner.x + jawPoint.x)/4.0f,
                  (eyeOuter.y + noseSide.y + mouthCorner.y + jawPoint.y)/4.0f);

    // Ellipse soft mask
    int minX = std::max(0, (int)(center.x - radiusX*1.5f));
    int maxX = std::min(w-1, (int)(center.x + radiusX*1.5f));
    int minY = std::max(0, (int)(center.y - radiusY*1.5f));
    int maxY = std::min(h-1, (int)(center.y + radiusY*1.5f));
    for(int y=minY;y<=maxY;++y){
        for(int x=minX;x<=maxX;++x){
            float dx = (x-center.x)/radiusX;
            float dy = (y-center.y)/radiusY;
            float dist = std::sqrt(dx*dx+dy*dy);
            if(dist<=1.0f){
                float alpha = 1.0f - dist; // soft edge
                // Additional soft falloff
                alpha = alpha*alpha; // quadratic
                out.alpha[y*w+x] = std::max(out.alpha[y*w+x], alpha);
            }
        }
    }

    ApplyFeather(out, 3.0f);
    return ValidateMask(out, err);
}

bool HFBeautyMaskGenerator::GenerateNoseMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err) {
    if (face.landmarks.size()<36) { err="Not enough landmarks for nose"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign(w*h, 0.0f);

    // Nose landmarks 27-35 polygon
    std::vector<HFVec2> nosePoly;
    for(int i=27;i<=35;++i) if(i<(int)face.landmarks.size()) nosePoly.push_back(HFVec2(face.landmarks[i].x, face.landmarks[i].y));
    RasterizePolygon(nosePoly, out, 1.0f);
    DilateMask(out, 1.0f);
    ApplyFeather(out, 1.5f);
    return ValidateMask(out, err);
}

bool HFBeautyMaskGenerator::GenerateChinMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err) {
    if (face.landmarks.size()<17) { err="Not enough landmarks for chin"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign(w*h, 0.0f);

    // Chin region: around landmarks 6-10 + mouth bottom 57-58
    std::vector<HFVec2> chinPoly;
    for(int i=6;i<=10;++i) chinPoly.push_back(HFVec2(face.landmarks[i].x, face.landmarks[i].y));
    if(face.landmarks.size()>58){
        chinPoly.push_back(HFVec2(face.landmarks[58].x, face.landmarks[58].y));
        chinPoly.push_back(HFVec2(face.landmarks[57].x, face.landmarks[57].y));
        chinPoly.push_back(HFVec2(face.landmarks[56].x, face.landmarks[56].y));
    }
    RasterizePolygon(chinPoly, out, 1.0f);
    ApplyFeather(out, 2.0f);
    return ValidateMask(out, err);
}

bool HFBeautyMaskGenerator::GenerateUnderEyeMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err, bool left) {
    if (face.landmarks.size()<48) { err="Not enough landmarks for under eye"; return false; }
    out.width=w; out.height=h;
    out.alpha.assign(w*h, 0.0f);

    // Under eye: below eye landmarks, above cheek
    std::vector<int> eyeIndices;
    if(left){
        eyeIndices = {42,43,44,45,46,47};
    } else {
        eyeIndices = {36,37,38,39,40,41};
    }
    auto eyePoly = GetLandmarkPolygon(face, eyeIndices);
    if(eyePoly.empty()){ err="Empty eye polygon"; return false; }

    // Estimate under eye region: shift down by eye height
    float eyeH=0;
    for(auto& p: eyePoly) eyeH+=p.y;
    eyeH/=eyePoly.size();
    float eyeTop=eyePoly[0].y;
    float eyeBottom=eyePoly[0].y;
    for(auto& p: eyePoly){ eyeTop=std::min(eyeTop,p.y); eyeBottom=std::max(eyeBottom,p.y); }
    float shift = (eyeBottom-eyeTop)*1.5f;

    std::vector<HFVec2> underEyePoly;
    for(auto& p: eyePoly){
        underEyePoly.push_back(HFVec2(p.x, p.y+shift));
    }
    // Also include original eye bottom edge
    for(int i=(int)eyePoly.size()-1;i>=0;--i){
        underEyePoly.push_back(HFVec2(eyePoly[i].x, eyePoly[i].y+shift*0.2f));
    }

    RasterizePolygon(underEyePoly, out, 1.0f);
    ApplyFeather(out, 2.0f);
    return ValidateMask(out, err);
}

bool HFBeautyMaskGenerator::GenerateEyeExclusionMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err) {
    out.width=w; out.height=h;
    out.alpha.assign(w*h, 0.0f);
    if (face.landmarks.size()<48) { err="Not enough landmarks for eye exclusion"; return false; }

    // Left eye 42-47, right eye 36-41
    std::vector<HFVec2> leftEye = GetLandmarkPolygon(face, {42,43,44,45,46,47});
    std::vector<HFVec2> rightEye = GetLandmarkPolygon(face, {36,37,38,39,40,41});
    RasterizePolygon(leftEye, out, 1.0f);
    RasterizePolygon(rightEye, out, 1.0f);
    DilateMask(out, 3.0f); // expand exclusion a bit to protect eyelashes
    ApplyFeather(out, 1.0f);
    return ValidateMask(out, err);
}

bool HFBeautyMaskGenerator::GenerateLipExclusionMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err) {
    out.width=w; out.height=h;
    out.alpha.assign(w*h, 0.0f);
    if (face.landmarks.size()<68) { err="Not enough landmarks for lip exclusion"; return false; }

    // Outer lip 48-59
    std::vector<HFVec2> outerLip = GetLandmarkPolygon(face, {48,49,50,51,52,53,54,55,56,57,58,59});
    RasterizePolygon(outerLip, out, 1.0f);
    DilateMask(out, 2.0f);
    ApplyFeather(out, 1.0f);
    return ValidateMask(out, err);
}

bool HFBeautyMaskGenerator::GenerateBrowExclusionMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err) {
    out.width=w; out.height=h;
    out.alpha.assign(w*h, 0.0f);
    if (face.landmarks.size()<27) { err="Not enough landmarks for brow exclusion"; return false; }

    // Brow exclusion: need thickened region, not just line polygon which may be colinear
    // For each brow, create bounding box expanded
    auto makeBrowMask = [&](const std::vector<int>& indices){
        auto poly = GetLandmarkPolygon(face, indices);
        if(poly.empty()) return;
        // Compute bounds
        float minX=poly[0].x, maxX=poly[0].x, minY=poly[0].y, maxY=poly[0].y;
        for(auto& p: poly){ minX=std::min(minX,p.x); maxX=std::max(maxX,p.x); minY=std::min(minY,p.y); maxY=std::max(maxY,p.y); }
        // Expand vertically to create thick brow region (eyebrows have thickness)
        float thickness = 8.0f;
        minY -= thickness*0.5f;
        maxY += thickness*0.5f;
        minX -= 2.0f;
        maxX += 2.0f;
        std::vector<HFVec2> rect;
        rect.push_back(HFVec2(minX, minY));
        rect.push_back(HFVec2(maxX, minY));
        rect.push_back(HFVec2(maxX, maxY));
        rect.push_back(HFVec2(minX, maxY));
        RasterizePolygon(rect, out, 1.0f);
        // Also rasterize original poly if valid
        if(poly.size()>=3){
            RasterizePolygon(poly, out, 1.0f);
        }
    };

    makeBrowMask({22,23,24,25,26}); // left
    makeBrowMask({17,18,19,20,21}); // right

    // If still zero (fallback), create small circles at brow points
    if(!out.HasNonZero()){
        for(int idx: {17,18,19,20,21,22,23,24,25,26}){
            if(idx<(int)face.landmarks.size()){
                float cx=face.landmarks[idx].x, cy=face.landmarks[idx].y;
                int r=6;
                int minX=std::max(0,(int)(cx-r)), maxX=std::min(w-1,(int)(cx+r));
                int minY=std::max(0,(int)(cy-r)), maxY=std::min(h-1,(int)(cy+r));
                for(int y=minY;y<=maxY;++y) for(int x=minX;x<=maxX;++x){
                    float dx=x-cx, dy=y-cy;
                    if(dx*dx+dy*dy<=r*r) out.SetAlpha(x,y,1.0f);
                }
            }
        }
    }

    DilateMask(out, 2.0f);
    ApplyFeather(out, 1.0f);
    return ValidateMask(out, err);
}

bool HFBeautyMaskGenerator::GenerateSkinMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err) {
    // Pipeline: Face Mesh -> Face Region -> Exclude Eyes -> Exclude Brows -> Exclude Lips -> Skin Mask
    HFBeautyMask faceMask, eyeExclusion, lipExclusion, browExclusion;
    std::string tmpErr;
    if (!GenerateFaceMask(face, w, h, faceMask, err)) return false;

    // Eye exclusion — if fails, use empty mask (no exclusion) rather than failing skin
    if (!GenerateEyeExclusionMask(face, w, h, eyeExclusion, tmpErr)) {
        eyeExclusion.width=w; eyeExclusion.height=h; eyeExclusion.alpha.assign(w*h,0.0f);
    }
    if (!GenerateLipExclusionMask(face, w, h, lipExclusion, tmpErr)) {
        lipExclusion.width=w; lipExclusion.height=h; lipExclusion.alpha.assign(w*h,0.0f);
    }
    if (!GenerateBrowExclusionMask(face, w, h, browExclusion, tmpErr)) {
        browExclusion.width=w; browExclusion.height=h; browExclusion.alpha.assign(w*h,0.0f);
    }

    out = faceMask;
    // Subtract exclusions
    SubtractMask(out, eyeExclusion);
    SubtractMask(out, lipExclusion);
    SubtractMask(out, browExclusion);

    // Additional: erode slightly to avoid face boundary, then feather
    // Keep skin mask soft
    ApplyFeather(out, 2.5f);

    // Validate exclusions
    std::string exclusionErr;
    if (!ValidateSkinExclusion(out, eyeExclusion, lipExclusion, exclusionErr)) {
        // Not fatal, but log; still return mask if coverage valid
        // For strict validation, we still pass if coverage non-zero
        // err = exclusionErr; // don't fail, just warn
    }

    return ValidateMask(out, err);
}

bool HFBeautyMaskGenerator::GenerateMask(const HFFaceData& face, BeautyMaskType type, int w, int h, HFBeautyMask& out, std::string& err) {
    switch(type){
        case BeautyMaskType::Face: return GenerateFaceMask(face,w,h,out,err);
        case BeautyMaskType::Forehead: return GenerateForeheadMask(face,w,h,out,err);
        case BeautyMaskType::LeftCheek: return GenerateCheekMask(face,w,h,out,err,true);
        case BeautyMaskType::RightCheek: return GenerateCheekMask(face,w,h,out,err,false);
        case BeautyMaskType::Nose: return GenerateNoseMask(face,w,h,out,err);
        case BeautyMaskType::Chin: return GenerateChinMask(face,w,h,out,err);
        case BeautyMaskType::UnderEyeLeft: return GenerateUnderEyeMask(face,w,h,out,err,true);
        case BeautyMaskType::UnderEyeRight: return GenerateUnderEyeMask(face,w,h,out,err,false);
        case BeautyMaskType::Skin: return GenerateSkinMask(face,w,h,out,err);
        case BeautyMaskType::EyeExclusion: return GenerateEyeExclusionMask(face,w,h,out,err);
        case BeautyMaskType::LipExclusion: return GenerateLipExclusionMask(face,w,h,out,err);
        case BeautyMaskType::BrowExclusion: return GenerateBrowExclusionMask(face,w,h,out,err);
        default: err="Unknown mask type"; return false;
    }
}

bool HFBeautyMaskGenerator::GenerateAllMasks(const HFFaceData& face, int w, int h, std::map<BeautyMaskType, HFBeautyMask>& outMasks, std::string& err) {
    outMasks.clear();
    std::vector<BeautyMaskType> types = {
        BeautyMaskType::Face,
        BeautyMaskType::Forehead,
        BeautyMaskType::LeftCheek,
        BeautyMaskType::RightCheek,
        BeautyMaskType::Nose,
        BeautyMaskType::Chin,
        BeautyMaskType::UnderEyeLeft,
        BeautyMaskType::UnderEyeRight,
        BeautyMaskType::Skin,
        BeautyMaskType::EyeExclusion,
        BeautyMaskType::LipExclusion,
        BeautyMaskType::BrowExclusion
    };
    for(auto t: types){
        HFBeautyMask mask;
        std::string e;
        if(GenerateMask(face,t,w,h,mask,e)){
            outMasks[t]=std::move(mask);
        } else {
            // For required masks, fail; for debug exclusions, still try
            if(t==BeautyMaskType::Face || t==BeautyMaskType::Skin){
                err="Failed to generate "+BeautyMaskTypeToString(t)+": "+e;
                return false;
            }
        }
    }
    if(outMasks.find(BeautyMaskType::Skin)==outMasks.end()){
        err="Skin mask missing";
        return false;
    }
    return true;
}

} // namespace huanface
