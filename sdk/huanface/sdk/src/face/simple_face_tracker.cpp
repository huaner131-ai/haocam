/**
 * HuanFace Simple Face Tracker Implementation — Phase 4
 * Heuristic: YCrCb skin detection + largest contour
 */

#include "simple_face_tracker.h"
#include "../frame/frame.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <queue>

namespace huanface {

HFResult SimpleFaceTracker::Init(const HFEngineConfigC& config) {
    minFaceRatio = config.minFaceRatio > 0 ? config.minFaceRatio : 0.1f;
    maxFaces = config.maxFaces > 0 ? config.maxFaces : 1;
    initialized = true;
    return HF_RESULT_OK;
}

void SimpleFaceTracker::Shutdown() {
    initialized = false;
}

bool SimpleFaceTracker::IsSkinPixel(uint8_t r, uint8_t g, uint8_t b) const {
    // Convert RGB to YCrCb
    // Y = 0.299R+0.587G+0.114B
    // Cb = 128 -0.168736R -0.331264G +0.5B
    // Cr = 128 +0.5R -0.418688G -0.081312B
    float fr = (float)r, fg = (float)g, fb = (float)b;
    float Y = 0.299f*fr + 0.587f*fg + 0.114f*fb;
    float Cb = 128.0f -0.168736f*fr -0.331264f*fg +0.5f*fb;
    float Cr = 128.0f +0.5f*fr -0.418688f*fg -0.081312f*fb;

    // Skin range heuristic (from literature)
    // Y > 80, Cr in [135,180], Cb in [85,135]
    // Also check RGB ordering: R > G > B or similar? For minimal we use YCrCb only
    if (Y < 80) return false;
    if (Cr < 135 || Cr > 180) return false;
    if (Cb < 85 || Cb > 135) return false;
    // Additional check: R > G and R > B and G > B? Helps reduce false positives
    if (!(r > 95 && g > 40 && b > 20 && r > g && r > b)) return false;
    return true;
}

bool SimpleFaceTracker::FindFaceBBox(const uint8_t* rgba, int width, int height, int stride, float& outX, float& outY, float& outW, float& outH, float& outConfidence) const {
    // Create skin mask binary
    std::vector<uint8_t> skinMask((size_t)width*height, 0);
    int skinCount = 0;
    for (int y=0;y<height;++y) {
        const uint8_t* row = rgba + y*stride;
        for (int x=0;x<width;++x) {
            const uint8_t* px = row + x*4;
            uint8_t r = px[0], g = px[1], b = px[2];
            if (IsSkinPixel(r,g,b)) {
                skinMask[y*width+x]=1;
                skinCount++;
            }
        }
    }
    if (skinCount < (width*height*0.01f)) {
        return false; // too little skin
    }

    // Find largest connected component via BFS flood fill (4-connected)
    std::vector<int> label((size_t)width*height, -1);
    int currentLabel = 0;
    struct Component {
        int minX, minY, maxX, maxY;
        int count;
    };
    std::vector<Component> components;

    std::queue<std::pair<int,int>> q;
    const int dx[4]={1,-1,0,0};
    const int dy[4]={0,0,1,-1};

    for (int y=0;y<height;++y) {
        for (int x=0;x<width;++x) {
            int idx = y*width+x;
            if (skinMask[idx]==0 || label[idx]!=-1) continue;
            // New component
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

    if (components.empty()) return false;

    // Find largest component that has reasonable aspect ratio for face
    // Face aspect ratio ~ 0.8 to 1.5 width/height, and size at least minFaceRatio of image
    Component* best = nullptr;
    int bestScore = 0;
    for (auto& comp : components) {
        int w = comp.maxX - comp.minX + 1;
        int h = comp.maxY - comp.minY + 1;
        if (w<=0||h<=0) continue;
        float aspect = (float)w / (float)h;
        if (aspect < 0.5f || aspect > 2.0f) continue; // too extreme
        float areaRatio = (float)(w*h) / (float)(width*height);
        if (areaRatio < minFaceRatio*0.1f) continue; // too small
        if (areaRatio > 0.8f) continue; // too large (likely background)
        // Score by count and central position
        int centerX = comp.minX + w/2;
        int centerY = comp.minY + h/2;
        // Prefer central
        float distFromCenter = sqrtf(powf((float)centerX - width/2.0f,2)+powf((float)centerY - height/2.0f,2));
        float centerScore = 1.0f - distFromCenter / (float)std::max(width,height);
        int score = (int)(comp.count * (0.5f + centerScore*0.5f));
        if (score > bestScore) {
            bestScore = score;
            best = &comp;
        }
    }

    if (!best) return false;

    outX = (float)best->minX;
    outY = (float)best->minY;
    outW = (float)(best->maxX - best->minX + 1);
    outH = (float)(best->maxY - best->minY + 1);
    // Confidence based on skin count vs bbox area and size
    float bboxArea = outW*outH;
    float fillRatio = (float)best->count / bboxArea;
    float sizeRatio = bboxArea / (float)(width*height);
    outConfidence = std::min(1.0f, fillRatio*0.7f + sizeRatio*3.0f);
    // Clamp confidence 0.3-0.95
    outConfidence = std::max(0.3f, std::min(0.95f, outConfidence));
    // Expand bbox slightly (face includes hair, chin)
    float expandX = outW*0.15f;
    float expandY = outH*0.2f;
    outX = std::max(0.0f, outX - expandX);
    outY = std::max(0.0f, outY - expandY*0.5f);
    outW = std::min((float)width - outX, outW + expandX*2);
    outH = std::min((float)height - outY, outH + expandY*1.5f);
    return true;
}

void SimpleFaceTracker::GenerateLandmarks(const HFFaceData& faceTemplate, int imageW, int imageH, HFFaceData& outFace) const {
    (void)imageW; (void)imageH;
    // Generate 5 landmarks + 68 approximated
    // 5: left eye, right eye, nose, left mouth, right mouth
    // Using bbox proportions
    float x = outFace.bboxX;
    float y = outFace.bboxY;
    float w = outFace.bboxW;
    float h = outFace.bboxH;

    // 5 landmarks
    std::vector<HFVec2> landmarks5;
    landmarks5.reserve(5);
    landmarks5.emplace_back(x + w*0.35f, y + h*0.4f); // left eye
    landmarks5.emplace_back(x + w*0.65f, y + h*0.4f); // right eye
    landmarks5.emplace_back(x + w*0.5f,  y + h*0.55f); // nose
    landmarks5.emplace_back(x + w*0.35f, y + h*0.75f); // left mouth
    landmarks5.emplace_back(x + w*0.65f, y + h*0.75f); // right mouth

    // 68 landmarks approximated: we generate using standard face layout
    // For minimal, we generate 68 points distributed within bbox
    // This is runtime-defined, not hardcoded 68/106 in API, but we generate 68 for compatibility
    // The API allows any count, we use 68 for Phase 4 minimal
    std::vector<HFVec2> landmarks68;
    landmarks68.reserve(68);

    // Jaw line 0-16: from top-left to top-right along bottom
    for (int i=0;i<17;++i) {
        float t = (float)i/16.0f;
        float lx = x + w*(0.15f + t*0.7f);
        // Jaw curves slightly down in middle
        float ly = y + h*(0.2f + 0.8f*(1.0f - 0.3f*sinf(t*3.14159f)));
        landmarks68.emplace_back(lx, ly);
    }
    // Right eyebrow 17-21
    for (int i=0;i<5;++i) {
        float t = (float)i/4.0f;
        float lx = x + w*(0.25f + t*0.15f);
        float ly = y + h*(0.3f - 0.02f*sinf(t*3.14159f));
        landmarks68.emplace_back(lx, ly);
    }
    // Left eyebrow 22-26
    for (int i=0;i<5;++i) {
        float t = (float)i/4.0f;
        float lx = x + w*(0.6f + t*0.15f);
        float ly = y + h*(0.3f - 0.02f*sinf(t*3.14159f));
        landmarks68.emplace_back(lx, ly);
    }
    // Nose bridge 27-30
    for (int i=0;i<4;++i) {
        float t = (float)i/3.0f;
        float lx = x + w*0.5f;
        float ly = y + h*(0.35f + t*0.2f);
        landmarks68.emplace_back(lx, ly);
    }
    // Nose tip 31-35
    for (int i=0;i<5;++i) {
        float t = (float)i/4.0f;
        float lx = x + w*(0.42f + t*0.16f);
        float ly = y + h*0.6f;
        landmarks68.emplace_back(lx, ly);
    }
    // Right eye 36-41 (6 points)
    {
        float ex = x + w*0.35f, ey = y + h*0.4f;
        float ew = w*0.12f, eh = h*0.04f;
        for (int i=0;i<6;++i) {
            float ang = (float)i/6.0f * 2*3.14159f;
            landmarks68.emplace_back(ex + cosf(ang)*ew*0.5f, ey + sinf(ang)*eh*0.5f);
        }
    }
    // Left eye 42-47
    {
        float ex = x + w*0.65f, ey = y + h*0.4f;
        float ew = w*0.12f, eh = h*0.04f;
        for (int i=0;i<6;++i) {
            float ang = (float)i/6.0f * 2*3.14159f;
            landmarks68.emplace_back(ex + cosf(ang)*ew*0.5f, ey + sinf(ang)*eh*0.5f);
        }
    }
    // Outer lip 48-59 (12 points)
    {
        float mx = x + w*0.5f, my = y + h*0.75f;
        float mw = w*0.25f, mh = h*0.08f;
        for (int i=0;i<12;++i) {
            float ang = (float)i/12.0f * 2*3.14159f;
            landmarks68.emplace_back(mx + cosf(ang)*mw*0.5f, my + sinf(ang)*mh*0.5f);
        }
    }
    // Inner lip 60-67 (8 points)
    {
        float mx = x + w*0.5f, my = y + h*0.75f;
        float mw = w*0.15f, mh = h*0.04f;
        for (int i=0;i<8;++i) {
            float ang = (float)i/8.0f * 2*3.14159f;
            landmarks68.emplace_back(mx + cosf(ang)*mw*0.5f, my + sinf(ang)*mh*0.5f);
        }
    }

    // For Phase 4, we use 68 landmarks as main, but also keep 5 as subset
    outFace.landmarks = landmarks68;
    // 3D landmarks: add z=0 for all, with slight depth for nose
    outFace.landmarks3D.reserve(landmarks68.size());
    for (size_t i=0;i<landmarks68.size();++i) {
        float z = 0;
        // Nose points have positive z
        if (i>=27 && i<=35) z = 10.0f;
        outFace.landmarks3D.emplace_back(landmarks68[i].x, landmarks68[i].y, z);
    }

    // Rotation, translation, scale
    outFace.rotationPitch = 0;
    outFace.rotationYaw = 0;
    outFace.rotationRoll = 0;
    outFace.translationX = x + w*0.5f;
    outFace.translationY = y + h*0.5f;
    outFace.translationZ = 0;
    outFace.scale = w / 200.0f; // normalized scale
}

void SimpleFaceTracker::GenerateMesh(HFFaceData& face, int imageW, int imageH) const {
    // Generate simple face mesh: 5x5 grid inside bbox, plus triangles
    // This is minimal real mesh, not hardcoded 68/106 but runtime-defined
    int gridW = 5, gridH = 5;
    int vertCount = gridW*gridH;
    face.mesh.vertices.reserve(vertCount);
    face.mesh.uv.reserve(vertCount);
    face.mesh.width = imageW;
    face.mesh.height = imageH;

    float x = face.bboxX, y = face.bboxY, w = face.bboxW, h = face.bboxH;
    for (int gy=0; gy<gridH; ++gy) {
        for (int gx=0; gx<gridW; ++gx) {
            float fx = (float)gx / (float)(gridW-1);
            float fy = (float)gy / (float)(gridH-1);
            float px = x + fx*w;
            float py = y + fy*h;
            float pz = 0;
            // Add some depth variation: center higher
            float dx = fx-0.5f, dy = fy-0.5f;
            float dist = sqrtf(dx*dx+dy*dy);
            pz = (1.0f - dist)*10.0f;
            face.mesh.vertices.emplace_back(px, py, pz);
            face.mesh.uv.emplace_back(px / (float)imageW, py / (float)imageH);
        }
    }
    // Indices for triangles: two per quad
    for (int gy=0; gy<gridH-1; ++gy) {
        for (int gx=0; gx<gridW-1; ++gx) {
            int i0 = gy*gridW + gx;
            int i1 = gy*gridW + gx+1;
            int i2 = (gy+1)*gridW + gx;
            int i3 = (gy+1)*gridW + gx+1;
            // Triangle 1: i0,i1,i2
            face.mesh.indices.push_back(i0);
            face.mesh.indices.push_back(i1);
            face.mesh.indices.push_back(i2);
            // Triangle 2: i1,i3,i2
            face.mesh.indices.push_back(i1);
            face.mesh.indices.push_back(i3);
            face.mesh.indices.push_back(i2);
        }
    }
}

HFResult SimpleFaceTracker::Process(const HFFrameC* input, HFTrackingData& outTracking) {
    if (!initialized) return HF_RESULT_NOT_INITIALIZED;
    if (!input) return HF_RESULT_INVALID_PARAM;

    std::string err;
    HFResult vr = FrameValidator::Validate(input, &err);
    if (vr != HF_RESULT_OK) return vr;

    if (input->format != HF_FORMAT_RGBA8 && input->format != HF_FORMAT_BGRA8) {
        // For minimal, only support RGBA8/BGRA8
        return HF_RESULT_NOT_SUPPORTED;
    }

    int width = input->width;
    int height = input->height;
    int stride = input->stride;
    const uint8_t* data = input->data;

    if (!data) return HF_RESULT_INVALID_PARAM;

    // Convert BGRA to RGBA if needed for skin detection (we need R,G,B order)
    // For minimal, assume RGBA if format is RGBA8, and BGRA if BGRA8
    std::vector<uint8_t> converted;
    const uint8_t* rgbaData = data;
    if (input->format == HF_FORMAT_BGRA8) {
        converted.resize((size_t)height*stride);
        for (int y=0;y<height;++y) {
            const uint8_t* srcRow = data + y*stride;
            uint8_t* dstRow = converted.data() + y*stride;
            for (int x=0;x<width;++x) {
                const uint8_t* srcPx = srcRow + x*4;
                uint8_t* dstPx = dstRow + x*4;
                dstPx[0]=srcPx[2]; // R
                dstPx[1]=srcPx[1]; // G
                dstPx[2]=srcPx[0]; // B
                dstPx[3]=srcPx[3]; // A
            }
        }
        rgbaData = converted.data();
    }

    float bx, by, bw, bh, conf;
    if (!FindFaceBBox(rgbaData, width, height, stride, bx, by, bw, bh, conf)) {
        outTracking.Clear();
        outTracking.timestampNanos = input->timestampNanos;
        return HF_RESULT_OK; // No face, not error
    }

    HFFaceData face;
    face.id = 0;
    face.bboxX = bx;
    face.bboxY = by;
    face.bboxW = bw;
    face.bboxH = bh;
    face.confidence = conf;

    GenerateLandmarks(face, width, height, face);
    GenerateMesh(face, width, height);

    outTracking.Clear();
    outTracking.faces.push_back(std::move(face));
    outTracking.timestampNanos = input->timestampNanos;

    return HF_RESULT_OK;
}

} // namespace huanface
