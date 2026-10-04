/**
 * Production Face Detector Implementation — Phase 5
 * Multi-face, skin + eye/mouth verification, real confidence
 */

#include "face_detector.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <queue>
#include <vector>

namespace huanface {

HFResult ProductionFaceDetector::Init(const HFEngineConfigC& config) {
    minFaceRatio = config.minFaceRatio > 0 ? config.minFaceRatio : 0.08f;
    maxFaces = config.maxFaces > 0 ? config.maxFaces : 5;
    detectSmallFace = config.detectSmallFace != 0;
    initialized = true;
    return HF_RESULT_OK;
}

void ProductionFaceDetector::Shutdown() {
    initialized = false;
}

bool ProductionFaceDetector::IsSkinPixel(uint8_t r, uint8_t g, uint8_t b) const {
    float fr = (float)r, fg = (float)g, fb = (float)b;
    float Y = 0.299f*fr + 0.587f*fg + 0.114f*fb;
    float Cb = 128.0f -0.168736f*fr -0.331264f*fg +0.5f*fb;
    float Cr = 128.0f +0.5f*fr -0.418688f*fg -0.081312f*fb;
    if (Y < 70) return false; // slightly lower for dark skin support
    if (Cr < 133 || Cr > 180) return false;
    if (Cb < 77 || Cb > 135) return false;
    if (!(r > 90 && g > 35 && b > 15 && r > g && r > b)) return false;
    return true;
}

bool ProductionFaceDetector::FindEyeCandidates(const uint8_t* rgba, int width, int height, int stride,
                                               float faceX, float faceY, float faceW, float faceH,
                                               HFVec2& leftEye, HFVec2& rightEye, float& confidence) const {
    // Search for darkest regions in upper half of face (eye regions)
    int fx = (int)faceX, fy = (int)faceY;
    int fw = (int)faceW, fh = (int)faceH;
    fx = std::max(0, fx); fy = std::max(0, fy);
    fw = std::min(fw, width - fx); fh = std::min(fh, height - fy);
    if (fw <= 20 || fh <= 20) return false;

    // Eye search regions: upper 40% of face, left and right halves
    int eyeRegionY0 = fy + (int)(fh * 0.2f);
    int eyeRegionY1 = fy + (int)(fh * 0.55f);
    int leftX0 = fx + (int)(fw * 0.15f);
    int leftX1 = fx + (int)(fw * 0.45f);
    int rightX0 = fx + (int)(fw * 0.55f);
    int rightX1 = fx + (int)(fw * 0.85f);

    eyeRegionY0 = std::max(0, eyeRegionY0); eyeRegionY1 = std::min(height-1, eyeRegionY1);
    leftX0 = std::max(0, leftX0); leftX1 = std::min(width-1, leftX1);
    rightX0 = std::max(0, rightX0); rightX1 = std::min(width-1, rightX1);

    // Find darkest pixel in left eye region
    int bestLeftDark = 255*3;
    int bestLeftX = leftX0 + (leftX1-leftX0)/2;
    int bestLeftY = eyeRegionY0 + (eyeRegionY1-eyeRegionY0)/2;
    for (int y=eyeRegionY0; y<=eyeRegionY1; ++y) {
        const uint8_t* row = rgba + y*stride;
        for (int x=leftX0; x<=leftX1; ++x) {
            const uint8_t* px = row + x*4;
            int intensity = (int)px[0] + (int)px[1] + (int)px[2]; // R+G+B lower = darker
            // Prefer dark but not too dark (eye pupil dark)
            if (intensity < bestLeftDark) {
                bestLeftDark = intensity;
                bestLeftX = x;
                bestLeftY = y;
            }
        }
    }

    int bestRightDark = 255*3;
    int bestRightX = rightX0 + (rightX1-rightX0)/2;
    int bestRightY = eyeRegionY0 + (eyeRegionY1-eyeRegionY0)/2;
    for (int y=eyeRegionY0; y<=eyeRegionY1; ++y) {
        const uint8_t* row = rgba + y*stride;
        for (int x=rightX0; x<=rightX1; ++x) {
            const uint8_t* px = row + x*4;
            int intensity = (int)px[0] + (int)px[1] + (int)px[2];
            if (intensity < bestRightDark) {
                bestRightDark = intensity;
                bestRightX = x;
                bestRightY = y;
            }
        }
    }

    // Validate eye distance and symmetry
    float eyeDist = std::abs((float)bestRightX - (float)bestLeftX);
    float expectedDist = fw * 0.3f; // eyes about 30% of face width apart
    if (eyeDist < fw*0.15f || eyeDist > fw*0.6f) {
        // fallback to estimated positions
        leftEye = HFVec2(fx + fw*0.35f, fy + fh*0.4f);
        rightEye = HFVec2(fx + fw*0.65f, fy + fh*0.4f);
        confidence = 0.3f;
        return true; // still provide estimate but low confidence
    }

    // Check that eyes are roughly horizontal (roll not too large)
    float eyeDy = std::abs((float)bestRightY - (float)bestLeftY);
    if (eyeDy > fh*0.15f) {
        // eyes too vertical, low confidence
        confidence = 0.4f;
    } else {
        confidence = 0.7f;
    }

    // Refine with average of dark region around best point (3x3)
    auto refineEye = [&](int bx, int by, int x0, int x1, int y0, int y1) -> HFVec2 {
        float sumX=0, sumY=0, sumW=0;
        int rad = 4;
        for (int dy=-rad; dy<=rad; ++dy) {
            for (int dx=-rad; dx<=rad; ++dx) {
                int nx = bx+dx, ny = by+dy;
                if (nx < x0 || nx > x1 || ny < y0 || ny > y1) continue;
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                const uint8_t* px = rgba + ny*stride + nx*4;
                int intensity = (int)px[0] + (int)px[1] + (int)px[2];
                float w = 1.0f / (1.0f + intensity/100.0f); // darker = higher weight
                sumX += nx * w;
                sumY += ny * w;
                sumW += w;
            }
        }
        if (sumW > 0) return HFVec2(sumX/sumW, sumY/sumW);
        return HFVec2((float)bx, (float)by);
    };

    leftEye = refineEye(bestLeftX, bestLeftY, leftX0, leftX1, eyeRegionY0, eyeRegionY1);
    rightEye = refineEye(bestRightX, bestRightY, rightX0, rightX1, eyeRegionY0, eyeRegionY1);

    // Confidence based on darkness and symmetry
    float darkConfLeft = 1.0f - (bestLeftDark / (float)(255*3));
    float darkConfRight = 1.0f - (bestRightDark / (float)(255*3));
    confidence = (darkConfLeft + darkConfRight) * 0.5f * 0.6f + confidence*0.4f;
    confidence = std::max(0.1f, std::min(1.0f, confidence));

    return true;
}

bool ProductionFaceDetector::FindMouthCandidate(const uint8_t* rgba, int width, int height, int stride,
                                                float faceX, float faceY, float faceW, float faceH,
                                                HFVec2& mouthCenter, float& confidence) const {
    int fx = (int)faceX, fy = (int)faceY;
    int fw = (int)faceW, fh = (int)faceH;
    fx = std::max(0, fx); fy = std::max(0, fy);
    fw = std::min(fw, width - fx); fh = std::min(fh, height - fy);
    if (fw <= 10 || fh <= 10) return false;

    // Mouth region: lower 30% of face, middle 60% width
    int mouthY0 = fy + (int)(fh * 0.6f);
    int mouthY1 = fy + (int)(fh * 0.9f);
    int mouthX0 = fx + (int)(fw * 0.2f);
    int mouthX1 = fx + (int)(fw * 0.8f);

    mouthY0 = std::max(0, mouthY0); mouthY1 = std::min(height-1, mouthY1);
    mouthX0 = std::max(0, mouthX0); mouthX1 = std::min(width-1, mouthX1);

    // Search for reddish region (mouth/lips have higher R)
    int bestR = 0;
    int bestX = mouthX0 + (mouthX1-mouthX0)/2;
    int bestY = mouthY0 + (mouthY1-mouthY0)/2;
    for (int y=mouthY0; y<=mouthY1; ++y) {
        const uint8_t* row = rgba + y*stride;
        for (int x=mouthX0; x<=mouthX1; ++x) {
            const uint8_t* px = row + x*4;
            int r = px[0], g = px[1], b = px[2];
            // Mouth: R relatively high, R > G, and R > B, and G not too high
            int redScore = r - (g+b)/2;
            if (redScore > bestR) {
                bestR = redScore;
                bestX = x;
                bestY = y;
            }
        }
    }

    if (bestR < 10) {
        // fallback
        mouthCenter = HFVec2(fx + fw*0.5f, fy + fh*0.75f);
        confidence = 0.3f;
        return true;
    }

    mouthCenter = HFVec2((float)bestX, (float)bestY);
    confidence = std::min(1.0f, bestR / 100.0f);
    confidence = std::max(0.2f, confidence);
    return true;
}

bool ProductionFaceDetector::VerifyFaceCandidate(const uint8_t* rgba, int width, int height, int stride,
                                                 const FaceDetection& candidate,
                                                 float& outEyeConfidence, float& outMouthConfidence) const {
    HFVec2 leftEye, rightEye, mouth;
    float eyeConf, mouthConf;
    bool hasEyes = FindEyeCandidates(rgba, width, height, stride,
                                     candidate.x, candidate.y, candidate.w, candidate.h,
                                     leftEye, rightEye, eyeConf);
    bool hasMouth = FindMouthCandidate(rgba, width, height, stride,
                                       candidate.x, candidate.y, candidate.w, candidate.h,
                                       mouth, mouthConf);
    outEyeConfidence = hasEyes ? eyeConf : 0.0f;
    outMouthConfidence = hasMouth ? mouthConf : 0.0f;
    // Face must have at least eyes or mouth with reasonable confidence
    return (eyeConf > 0.2f || mouthConf > 0.2f);
}

HFResult ProductionFaceDetector::Detect(const uint8_t* rgba, int width, int height, int stride,
                                        std::vector<FaceDetection>& outFaces) {
    if (!initialized) return HF_RESULT_NOT_INITIALIZED;
    if (!rgba) return HF_RESULT_INVALID_PARAM;
    outFaces.clear();

    // Create skin mask
    std::vector<uint8_t> skinMask((size_t)width*height, 0);
    int skinCount = 0;
    for (int y=0; y<height; ++y) {
        const uint8_t* row = rgba + y*stride;
        for (int x=0; x<width; ++x) {
            const uint8_t* px = row + x*4;
            if (IsSkinPixel(px[0], px[1], px[2])) {
                skinMask[y*width+x]=1;
                skinCount++;
            }
        }
    }

    if (skinCount < (width*height*0.005f) && !detectSmallFace) {
        return HF_RESULT_OK; // no face
    }

    // Connected components for multi-face
    std::vector<int> label((size_t)width*height, -1);
    struct Component {
        int minX, minY, maxX, maxY;
        int count;
    };
    std::vector<Component> components;

    std::queue<std::pair<int,int>> q;
    const int dx[4]={1,-1,0,0};
    const int dy[4]={0,0,1,-1};
    int currentLabel = 0;

    for (int y=0; y<height; ++y) {
        for (int x=0; x<width; ++x) {
            int idx = y*width+x;
            if (skinMask[idx]==0 || label[idx]!=-1) continue;
            Component comp;
            comp.minX=x; comp.maxX=x; comp.minY=y; comp.maxY=y; comp.count=0;
            q.emplace(x,y);
            label[idx]=currentLabel;
            while (!q.empty()) {
                auto [cx,cy]=q.front(); q.pop();
                comp.minX = std::min(comp.minX, cx);
                comp.maxX = std::max(comp.maxX, cx);
                comp.minY = std::min(comp.minY, cy);
                comp.maxY = std::max(comp.maxY, cy);
                comp.count++;
                for (int k=0;k<4;++k) {
                    int nx=cx+dx[k], ny=cy+dy[k];
                    if (nx<0||nx>=width||ny<0||ny>=height) continue;
                    int nidx=ny*width+nx;
                    if (skinMask[nidx]==0 || label[nidx]!=-1) continue;
                    label[nidx]=currentLabel;
                    q.emplace(nx,ny);
                }
            }
            components.push_back(comp);
            currentLabel++;
        }
    }

    if (components.empty()) {
        return HF_RESULT_OK;
    }

    // Score components and create face candidates
    struct ScoredFace {
        Component comp;
        float score;
        FaceDetection detection;
    };
    std::vector<ScoredFace> scored;

    for (auto& comp : components) {
        int w = comp.maxX - comp.minX + 1;
        int h = comp.maxY - comp.minY + 1;
        if (w<=0||h<=0) continue;
        float aspect = (float)w / (float)h;
        if (aspect < 0.4f || aspect > 2.5f) continue;
        float areaRatio = (float)(w*h) / (float)(width*height);
        float minArea = detectSmallFace ? 0.005f : minFaceRatio*0.05f;
        if (areaRatio < minArea) continue;
        if (areaRatio > 0.9f) continue;

        float bboxArea = (float)w*h;
        float fillRatio = (float)comp.count / bboxArea;
        if (fillRatio < 0.15f) continue; // too sparse

        // Expand bbox
        float ex = comp.minX - w*0.12f;
        float ey = comp.minY - h*0.15f;
        float ew = w*1.24f;
        float eh = h*1.35f;
        ex = std::max(0.0f, ex);
        ey = std::max(0.0f, ey);
        ew = std::min((float)width - ex, ew);
        eh = std::min((float)height - ey, eh);

        FaceDetection det;
        det.x = ex; det.y = ey; det.w = ew; det.h = eh;

        float eyeConf, mouthConf;
        bool verified = VerifyFaceCandidate(rgba, width, height, stride, det, eyeConf, mouthConf);
        if (!verified) {
            // still allow but with lower confidence if fillRatio high and central
            if (fillRatio < 0.3f) continue;
        }

        // Confidence based on fillRatio, eye, mouth, size, centrality
        float centerX = ex + ew/2;
        float centerY = ey + eh/2;
        float distFromCenter = sqrtf(powf(centerX - width/2.0f,2)+powf(centerY - height/2.0f,2));
        float centerScore = 1.0f - distFromCenter / (float)std::max(width,height);
        centerScore = std::max(0.0f, centerScore);

        float sizeScore = std::min(1.0f, areaRatio * 10.0f);
        float skinScore = std::min(1.0f, fillRatio);

        float conf = skinScore*0.3f + eyeConf*0.35f + mouthConf*0.2f + centerScore*0.1f + sizeScore*0.05f;
        conf = std::max(0.15f, std::min(0.98f, conf));

        // Boost if has both eyes and mouth
        if (eyeConf > 0.4f && mouthConf > 0.3f) conf = std::min(0.98f, conf*1.2f);

        det.confidence = conf;
        det.hasEyes = eyeConf > 0.3f;
        det.hasMouth = mouthConf > 0.2f;
        det.eyeDistance = ew * 0.3f;

        float score = conf * comp.count * (0.5f + centerScore*0.5f);
        scored.push_back({comp, score, det});
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(), [](const ScoredFace& a, const ScoredFace& b){
        return a.score > b.score;
    });

    // Non-maximum suppression for overlapping faces
    std::vector<FaceDetection> finalFaces;
    for (auto& sf : scored) {
        bool overlap = false;
        for (auto& existing : finalFaces) {
            float iou = sf.detection.IoU(existing);
            if (iou > 0.3f) { overlap = true; break; }
        }
        if (!overlap) {
            finalFaces.push_back(sf.detection);
            if ((int)finalFaces.size() >= maxFaces) break;
        }
    }

    // Sort by confidence descending for consistent order
    std::sort(finalFaces.begin(), finalFaces.end(), [](const FaceDetection& a, const FaceDetection& b){
        return a.confidence > b.confidence;
    });

    outFaces = finalFaces;
    return HF_RESULT_OK;
}

} // namespace huanface
