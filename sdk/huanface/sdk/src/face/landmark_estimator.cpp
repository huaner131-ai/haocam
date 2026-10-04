/**
 * Production Landmark Estimator Implementation — Phase 5
 * Real image-based detection, not synthetic sin/cos
 */

#include "landmark_estimator.h"
#include <cmath>
#include <algorithm>

namespace huanface {

HFResult ProductionLandmarkEstimator::Init(const HFEngineConfigC& config) {
    (void)config;
    initialized = true;
    return HF_RESULT_OK;
}

void ProductionLandmarkEstimator::Shutdown() {
    initialized = false;
}

bool ProductionLandmarkEstimator::DetectEyeCenters(const uint8_t* rgba, int width, int height, int stride,
                                                    const FaceDetection& face,
                                                    HFVec2& leftEye, HFVec2& rightEye,
                                                    float& leftConf, float& rightConf) const {
    // Reuse logic from face_detector but more precise
    int fx = (int)face.x, fy = (int)face.y;
    int fw = (int)face.w, fh = (int)face.h;
    fx = std::max(0, fx); fy = std::max(0, fy);
    fw = std::min(fw, width - fx); fh = std::min(fh, height - fy);
    if (fw < 10 || fh < 10) return false;

    int eyeY0 = fy + (int)(fh * 0.22f);
    int eyeY1 = fy + (int)(fh * 0.52f);
    int leftX0 = fx + (int)(fw * 0.15f);
    int leftX1 = fx + (int)(fw * 0.45f);
    int rightX0 = fx + (int)(fw * 0.55f);
    int rightX1 = fx + (int)(fw * 0.85f);

    eyeY0 = std::max(0, eyeY0); eyeY1 = std::min(height-1, eyeY1);
    leftX0 = std::max(0, leftX0); leftX1 = std::min(width-1, leftX1);
    rightX0 = std::max(0, rightX0); rightX1 = std::min(width-1, rightX1);

    // Search darkest in left eye region with weighted average
    auto findDarkest = [&](int x0, int x1, int y0, int y1) -> std::pair<HFVec2, float> {
        int bestDark = 765;
        int bx = x0 + (x1-x0)/2, by = y0 + (y1-y0)/2;
        for (int y=y0; y<=y1; ++y) {
            const uint8_t* row = rgba + y*stride;
            for (int x=x0; x<=x1; ++x) {
                const uint8_t* px = row + x*4;
                int intensity = (int)px[0] + (int)px[1] + (int)px[2];
                if (intensity < bestDark) {
                    bestDark = intensity;
                    bx = x; by = y;
                }
            }
        }
        // Weighted average around darkest point 5x5
        float sumX=0, sumY=0, sumW=0;
        int rad=5;
        for (int dy=-rad; dy<=rad; ++dy) {
            for (int dx=-rad; dx<=rad; ++dx) {
                int nx=bx+dx, ny=by+dy;
                if (nx < x0 || nx > x1 || ny < y0 || ny > y1) continue;
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                const uint8_t* px = rgba + ny*stride + nx*4;
                int intensity = (int)px[0] + (int)px[1] + (int)px[2];
                float w = 1.0f / (1.0f + intensity/80.0f);
                sumX += nx * w;
                sumY += ny * w;
                sumW += w;
            }
        }
        HFVec2 pos((float)bx, (float)by);
        if (sumW > 0) pos = HFVec2(sumX/sumW, sumY/sumW);
        float conf = 1.0f - (bestDark / 765.0f);
        conf = std::max(0.1f, std::min(1.0f, conf));
        return {pos, conf};
    };

    auto leftRes = findDarkest(leftX0, leftX1, eyeY0, eyeY1);
    auto rightRes = findDarkest(rightX0, rightX1, eyeY0, eyeY1);

    leftEye = leftRes.first;
    rightEye = rightRes.first;
    leftConf = leftRes.second;
    rightConf = rightRes.second;

    // Validate eye distance
    float eyeDist = std::abs(rightEye.x - leftEye.x);
    if (eyeDist < fw*0.15f || eyeDist > fw*0.6f) {
        // fallback to estimated but lower confidence
        leftEye = HFVec2(fx + fw*0.35f, fy + fh*0.4f);
        rightEye = HFVec2(fx + fw*0.65f, fy + fh*0.4f);
        leftConf *= 0.5f;
        rightConf *= 0.5f;
    }

    return true;
}

bool ProductionLandmarkEstimator::DetectNose(const uint8_t* rgba, int width, int height, int stride,
                                              const FaceDetection& face,
                                              const HFVec2& leftEye, const HFVec2& rightEye,
                                              HFVec2& noseTip, float& conf) const {
    int fx = (int)face.x, fy = (int)face.y;
    int fw = (int)face.w, fh = (int)face.h;
    // Nose region: between eyes and mouth, middle of face
    int noseX0 = fx + (int)(fw * 0.35f);
    int noseX1 = fx + (int)(fw * 0.65f);
    int noseY0 = fy + (int)(fh * 0.45f);
    int noseY1 = fy + (int)(fh * 0.68f);

    noseX0 = std::max(0, noseX0); noseX1 = std::min(width-1, noseX1);
    noseY0 = std::max(0, noseY0); noseY1 = std::min(height-1, noseY1);

    // Eye midpoint
    float eyeMidX = (leftEye.x + rightEye.x) * 0.5f;
    // float eyeMidY = (leftEye.y + rightEye.y) * 0.5f;

    // Search for nose: slightly brighter than surroundings, central
    // Use vertical edge or brightness: nose bridge has highlight
    // Prefer central position in nose region
    int expectedNoseY = noseY0 + (noseY1 - noseY0) * 60 / 100; // 60% down in nose region ~ 0.55-0.6 of face
    int bestScore = -10000;
    int bx = (int)eyeMidX;
    int by = expectedNoseY;
    for (int y=noseY0; y<=noseY1; ++y) {
        const uint8_t* row = rgba + y*stride;
        for (int x=noseX0; x<=noseX1; ++x) {
            const uint8_t* px = row + x*4;
            int r = px[0], g = px[1], b = px[2];
            int brightness = (r+g+b)/3;
            float distFromCenterX = std::abs(x - eyeMidX);
            float distFromCenterY = std::abs(y - expectedNoseY);
            int centerScore = (int)(80 - distFromCenterX*0.8f - distFromCenterY*0.5f);
            int score = brightness + centerScore;
            if (score > bestScore) {
                bestScore = score;
                bx = x; by = y;
            }
        }
    }

    noseTip = HFVec2((float)bx, (float)by);
    conf = std::min(1.0f, std::max(0.2f, bestScore / 255.0f));
    return true;
}

bool ProductionLandmarkEstimator::DetectMouth(const uint8_t* rgba, int width, int height, int stride,
                                              const FaceDetection& face,
                                              HFVec2& mouthCenter, HFVec2& leftCorner, HFVec2& rightCorner,
                                              float& conf) const {
    int fx = (int)face.x, fy = (int)face.y;
    int fw = (int)face.w, fh = (int)face.h;
    int mouthY0 = fy + (int)(fh * 0.60f);
    int mouthY1 = fy + (int)(fh * 0.92f);
    int mouthX0 = fx + (int)(fw * 0.20f);
    int mouthX1 = fx + (int)(fw * 0.80f);

    mouthY0 = std::max(0, mouthY0); mouthY1 = std::min(height-1, mouthY1);
    mouthX0 = std::max(0, mouthX0); mouthX1 = std::min(width-1, mouthX1);

    // Search for mouth: red-ish region, higher R than G,B
    int bestRed = -1000;
    int bestX = mouthX0 + (mouthX1-mouthX0)/2;
    int bestY = mouthY0 + (mouthY1-mouthY0)/2;
    for (int y=mouthY0; y<=mouthY1; ++y) {
        const uint8_t* row = rgba + y*stride;
        for (int x=mouthX0; x<=mouthX1; ++x) {
            const uint8_t* px = row + x*4;
            int r = px[0], g = px[1], b = px[2];
            int redScore = r - (g+b)/2;
            // Also prefer darker than skin for mouth opening
            if (redScore > bestRed) {
                bestRed = redScore;
                bestX = x; bestY = y;
            }
        }
    }

    mouthCenter = HFVec2((float)bestX, (float)bestY);
    // Estimate corners: search left and right of center for mouth edges
    int leftBest = bestRed;
    int leftX = bestX;
    for (int x=bestX; x>=mouthX0; --x) {
        const uint8_t* px = rgba + bestY*stride + x*4;
        int r = px[0], g = px[1], b = px[2];
        int redScore = r - (g+b)/2;
        if (redScore < leftBest*0.5f) break;
        leftX = x;
    }
    int rightX = bestX;
    for (int x=bestX; x<=mouthX1; ++x) {
        const uint8_t* px = rgba + bestY*stride + x*4;
        int r = px[0], g = px[1], b = px[2];
        int redScore = r - (g+b)/2;
        if (redScore < bestRed*0.5f) break;
        rightX = x;
    }

    // If corners too close, use proportional fallback
    float cornerDist = rightX - leftX;
    if (cornerDist < fw*0.1f) {
        leftX = bestX - (int)(fw*0.12f);
        rightX = bestX + (int)(fw*0.12f);
    }

    leftCorner = HFVec2((float)leftX, (float)bestY);
    rightCorner = HFVec2((float)rightX, (float)bestY);

    conf = std::min(1.0f, std::max(0.15f, bestRed / 80.0f));
    return true;
}

bool ProductionLandmarkEstimator::DetectEyebrows(const uint8_t* rgba, int width, int height, int stride,
                                                  const FaceDetection& face,
                                                  const HFVec2& leftEye, const HFVec2& rightEye,
                                                  HFVec2& leftBrow, HFVec2& rightBrow,
                                                  float& leftConf, float& rightConf) const {
    // Eyebrows are above eyes, slightly darker than skin
    int fx = (int)face.x, fy = (int)face.y;
    int fw = (int)face.w, fh = (int)face.h;

    int browY0 = fy + (int)(fh * 0.15f);
    int browY1 = fy + (int)(fh * 0.35f);
    int leftX0 = fx + (int)(fw * 0.15f);
    int leftX1 = fx + (int)(fw * 0.45f);
    int rightX0 = fx + (int)(fw * 0.55f);
    int rightX1 = fx + (int)(fw * 0.85f);

    browY0 = std::max(0, browY0); browY1 = std::min(height-1, browY1);
    leftX0 = std::max(0, leftX0); leftX1 = std::min(width-1, leftX1);
    rightX0 = std::max(0, rightX0); rightX1 = std::min(width-1, rightX1);

    auto findBrow = [&](int x0, int x1, int y0, int y1, const HFVec2& eye) -> std::pair<HFVec2,float> {
        int bestDark = 765;
        int bx = x0 + (x1-x0)/2;
        int by = y0 + (y1-y0)/2;
        for (int y=y0; y<=y1; ++y) {
            const uint8_t* row = rgba + y*stride;
            for (int x=x0; x<=x1; ++x) {
                const uint8_t* px = row + x*4;
                int intensity = (int)px[0] + (int)px[1] + (int)px[2];
                // Brow is darker than skin but above eye
                if (y < (int)eye.y && intensity < bestDark) {
                    bestDark = intensity;
                    bx = x; by = y;
                }
            }
        }
        HFVec2 pos((float)bx, (float)by);
        float conf = 1.0f - (bestDark / 765.0f);
        return {pos, std::max(0.1f, std::min(1.0f, conf))};
    };

    auto leftRes = findBrow(leftX0, leftX1, browY0, browY1, leftEye);
    auto rightRes = findBrow(rightX0, rightX1, browY0, browY1, rightEye);

    leftBrow = leftRes.first;
    rightBrow = rightRes.first;
    leftConf = leftRes.second;
    rightConf = rightRes.second;

    // Fallback if too close to eye
    if (leftEye.y - leftBrow.y < fh*0.03f) {
        leftBrow = HFVec2(leftEye.x, leftEye.y - fh*0.1f);
        leftConf *= 0.5f;
    }
    if (rightEye.y - rightBrow.y < fh*0.03f) {
        rightBrow = HFVec2(rightEye.x, rightEye.y - fh*0.1f);
        rightConf *= 0.5f;
    }

    return true;
}

float ProductionLandmarkEstimator::EstimateDepth(const HFVec2& pt, const FaceDetection& face, const std::string& region) const {
    (void)pt;
    // Simple depth model: nose tip highest, eyes slightly, mouth lower
    if (region == "nose_tip") return 12.0f;
    if (region == "nose_bridge") return 8.0f;
    if (region == "eye") return 2.0f;
    if (region == "brow") return 1.0f;
    if (region == "lip") return 3.0f;
    if (region == "chin") return -2.0f;
    if (region == "cheek") return 0.0f;
    return 0.0f;
}

void ProductionLandmarkEstimator::Build68Landmarks(const FaceDetection& face,
                                                    const HFVec2& leftEye, const HFVec2& rightEye,
                                                    const HFVec2& noseTip,
                                                    const HFVec2& mouthCenter, const HFVec2& leftMouth, const HFVec2& rightMouth,
                                                    const HFVec2& leftBrow, const HFVec2& rightBrow,
                                                    std::vector<HFVec2>& outLandmarks) const {
    outLandmarks.clear();
    outLandmarks.reserve(68);

    float fx = face.x, fy = face.y, fw = face.w, fh = face.h;
    // Eye centers from real detection
    HFVec2 le = leftEye;
    HFVec2 re = rightEye;
    HFVec2 nose = noseTip;
    HFVec2 mouth = mouthCenter;
    HFVec2 lMouth = leftMouth;
    HFVec2 rMouth = rightMouth;
    HFVec2 lBrow = leftBrow;
    HFVec2 rBrow = rightBrow;

    // Jaw line 0-16: from left to right along face contour, anchored to real face bbox but shaped by eye/mouth
    // Use face bbox as base, but make it follow real face width
    for (int i=0;i<17;++i) {
        float t = (float)i/16.0f;
        float x = fx + fw*(0.12f + t*0.76f);
        // Jaw curve: lower in middle, based on mouth position
        float jawYBase = fy + fh*0.92f;
        float mouthInfluence = (mouth.y - (fy+fh*0.75f)) * 0.2f;
        float y = jawYBase + mouthInfluence - 0.15f*fh*std::sin(t*3.14159f);
        // Adjust for real mouth corners width
        if (t < 0.5f) {
            float leftFactor = (0.5f - t)*2.0f;
            x = x*(1-leftFactor*0.1f) + lMouth.x*leftFactor*0.1f;
        } else {
            float rightFactor = (t-0.5f)*2.0f;
            x = x*(1-rightFactor*0.1f) + rMouth.x*rightFactor*0.1f;
        }
        outLandmarks.emplace_back(x, y);
    }

    // Right eyebrow 17-21: anchored to real right brow detection
    for (int i=0;i<5;++i) {
        float t = (float)i/4.0f;
        float x = rBrow.x + (t-0.5f)*fw*0.18f;
        float y = rBrow.y + std::sin(t*3.14159f)*fh*0.01f - fh*0.01f;
        outLandmarks.emplace_back(x, y);
    }

    // Left eyebrow 22-26: anchored to real left brow
    for (int i=0;i<5;++i) {
        float t = (float)i/4.0f;
        float x = lBrow.x + (t-0.5f)*fw*0.18f;
        float y = lBrow.y + std::sin(t*3.14159f)*fh*0.01f - fh*0.01f;
        outLandmarks.emplace_back(x, y);
    }

    // Nose bridge 27-30: from between eyes to nose tip, anchored to real eyes and nose
    HFVec2 eyeMid((le.x+re.x)*0.5f, (le.y+re.y)*0.5f);
    for (int i=0;i<4;++i) {
        float t = (float)i/3.0f;
        float x = eyeMid.x + (nose.x - eyeMid.x)*t;
        float y = eyeMid.y + (nose.y - eyeMid.y)*t;
        outLandmarks.emplace_back(x, y);
    }

    // Nose tip 31-35: around real nose tip
    for (int i=0;i<5;++i) {
        float t = (float)i/4.0f;
        float x = nose.x + (t-0.5f)*fw*0.12f;
        float y = nose.y + (i==2 ? 0 : (i<2 ? -fh*0.02f : fh*0.02f));
        outLandmarks.emplace_back(x, y);
    }

    // Right eye 36-41: 6 points around real right eye center
    {
        float ew = fw*0.11f;
        float eh = fh*0.035f;
        for (int i=0;i<6;++i) {
            float ang = (float)i/6.0f * 2*3.14159f;
            float x = re.x + std::cos(ang)*ew*0.5f;
            float y = re.y + std::sin(ang)*eh*0.5f;
            outLandmarks.emplace_back(x, y);
        }
    }

    // Left eye 42-47: around real left eye
    {
        float ew = fw*0.11f;
        float eh = fh*0.035f;
        for (int i=0;i<6;++i) {
            float ang = (float)i/6.0f * 2*3.14159f;
            float x = le.x + std::cos(ang)*ew*0.5f;
            float y = le.y + std::sin(ang)*eh*0.5f;
            outLandmarks.emplace_back(x, y);
        }
    }

    // Outer lip 48-60: 12+1? Actually 48-59 =12 points, but spec says 48-60 inclusive is 13? We use 12 for compatibility
    // We'll generate 12 points around mouth, anchored to real mouth corners
    {
        float mw = std::abs(rMouth.x - lMouth.x);
        if (mw < fw*0.1f) mw = fw*0.25f;
        float mh = fh*0.08f;
        HFVec2 center = mouth;
        // Ensure center is between corners
        center.x = (lMouth.x + rMouth.x)*0.5f;
        for (int i=0;i<12;++i) {
            float ang = (float)i/12.0f * 2*3.14159f;
            float x = center.x + std::cos(ang)*mw*0.5f;
            float y = center.y + std::sin(ang)*mh*0.5f;
            outLandmarks.emplace_back(x, y);
        }
    }

    // Inner lip 60-67: 8 points inside outer lip
    {
        float mw = std::abs(rMouth.x - lMouth.x) * 0.6f;
        if (mw < fw*0.05f) mw = fw*0.15f;
        float mh = fh*0.03f;
        HFVec2 center = mouth;
        center.x = (lMouth.x + rMouth.x)*0.5f;
        for (int i=0;i<8;++i) {
            float ang = (float)i/8.0f * 2*3.14159f;
            float x = center.x + std::cos(ang)*mw*0.5f;
            float y = center.y + std::sin(ang)*mh*0.5f;
            outLandmarks.emplace_back(x, y);
        }
    }
}

HFResult ProductionLandmarkEstimator::Estimate(const uint8_t* rgba, int width, int height, int stride,
                                                const FaceDetection& face,
                                                std::vector<HFVec2>& outLandmarks,
                                                std::vector<HFVec3>& outLandmarks3D,
                                                std::vector<float>& outConfidences) {
    if (!initialized) return HF_RESULT_NOT_INITIALIZED;
    if (!rgba) return HF_RESULT_INVALID_PARAM;

    HFVec2 leftEye, rightEye, noseTip, mouthCenter, leftMouth, rightMouth, leftBrow, rightBrow;
    float leftEyeConf, rightEyeConf, noseConf, mouthConf, leftBrowConf, rightBrowConf;

    bool okEyes = DetectEyeCenters(rgba, width, height, stride, face, leftEye, rightEye, leftEyeConf, rightEyeConf);
    bool okNose = DetectNose(rgba, width, height, stride, face, leftEye, rightEye, noseTip, noseConf);
    bool okMouth = DetectMouth(rgba, width, height, stride, face, mouthCenter, leftMouth, rightMouth, mouthConf);
    bool okBrow = DetectEyebrows(rgba, width, height, stride, face, leftEye, rightEye, leftBrow, rightBrow, leftBrowConf, rightBrowConf);

    if (!okEyes) {
        leftEye = HFVec2(face.x + face.w*0.35f, face.y + face.h*0.4f);
        rightEye = HFVec2(face.x + face.w*0.65f, face.y + face.h*0.4f);
        leftEyeConf = rightEyeConf = 0.2f;
    }
    if (!okNose) {
        noseTip = HFVec2(face.x + face.w*0.5f, face.y + face.h*0.6f);
        noseConf = 0.2f;
    }
    if (!okMouth) {
        mouthCenter = HFVec2(face.x + face.w*0.5f, face.y + face.h*0.75f);
        leftMouth = HFVec2(face.x + face.w*0.35f, face.y + face.h*0.75f);
        rightMouth = HFVec2(face.x + face.w*0.65f, face.y + face.h*0.75f);
        mouthConf = 0.2f;
    }
    if (!okBrow) {
        leftBrow = HFVec2(leftEye.x, leftEye.y - face.h*0.1f);
        rightBrow = HFVec2(rightEye.x, rightEye.y - face.h*0.1f);
        leftBrowConf = rightBrowConf = 0.2f;
    }

    // Build 68 landmarks anchored to real detections
    Build68Landmarks(face, leftEye, rightEye, noseTip, mouthCenter, leftMouth, rightMouth, leftBrow, rightBrow, outLandmarks);

    // Build 3D and confidences
    outLandmarks3D.clear();
    outConfidences.clear();
    outLandmarks3D.reserve(outLandmarks.size());
    outConfidences.reserve(outLandmarks.size());

    for (size_t i=0; i<outLandmarks.size(); ++i) {
        float z = 0.0f;
        float conf = 0.5f;
        // Assign depth and confidence based on region
        if (i >= 0 && i <= 16) { z = EstimateDepth(outLandmarks[i], face, "chin"); conf = 0.6f; }
        else if (i >= 17 && i <= 21) { z = EstimateDepth(outLandmarks[i], face, "brow"); conf = rightBrowConf; }
        else if (i >= 22 && i <= 26) { z = EstimateDepth(outLandmarks[i], face, "brow"); conf = leftBrowConf; }
        else if (i >= 27 && i <= 30) { z = EstimateDepth(outLandmarks[i], face, "nose_bridge"); conf = noseConf; }
        else if (i >= 31 && i <= 35) { z = EstimateDepth(outLandmarks[i], face, "nose_tip"); conf = noseConf; }
        else if (i >= 36 && i <= 41) { z = EstimateDepth(outLandmarks[i], face, "eye"); conf = rightEyeConf; }
        else if (i >= 42 && i <= 47) { z = EstimateDepth(outLandmarks[i], face, "eye"); conf = leftEyeConf; }
        else if (i >= 48 && i <= 59) { z = EstimateDepth(outLandmarks[i], face, "lip"); conf = mouthConf; }
        else if (i >= 60 && i <= 67) { z = EstimateDepth(outLandmarks[i], face, "lip"); conf = mouthConf*0.9f; }

        outLandmarks3D.emplace_back(outLandmarks[i].x, outLandmarks[i].y, z);
        outConfidences.push_back(conf);
    }

    return HF_RESULT_OK;
}

} // namespace huanface
