/**
 * Production Face Mesh Generator Implementation — Phase 5
 * Real mesh following face shape, topology documented
 */

#include "face_mesh_generator.h"
#include <cmath>
#include <algorithm>

namespace huanface {

HFResult ProductionFaceMeshGenerator::Init(const HFEngineConfigC& config) {
    (void)config;
    initialized = true;
    return HF_RESULT_OK;
}

void ProductionFaceMeshGenerator::Shutdown() {
    initialized = false;
}

FaceRegion ProductionFaceMeshGenerator::GetRegionForLandmark(int idx) const {
    if (idx >=0 && idx <=16) return FaceRegion::JAW;
    if (idx >=17 && idx <=21) return FaceRegion::RIGHT_BROW;
    if (idx >=22 && idx <=26) return FaceRegion::LEFT_BROW;
    if (idx >=27 && idx <=30) return FaceRegion::NOSE_BRIDGE;
    if (idx >=31 && idx <=35) return FaceRegion::NOSE_TIP;
    if (idx >=36 && idx <=41) return FaceRegion::RIGHT_EYE;
    if (idx >=42 && idx <=47) return FaceRegion::LEFT_EYE;
    if (idx >=48 && idx <=60) return FaceRegion::OUTER_LIP;
    if (idx >=61 && idx <=67) return FaceRegion::INNER_LIP;
    return FaceRegion::FACE;
}

void ProductionFaceMeshGenerator::GenerateAdditionalVertices(const std::vector<HFVec2>& landmarks,
                                                              const FaceDetection& face,
                                                              std::vector<HFVec3>& vertices,
                                                              std::vector<HFVec2UV>& uvs,
                                                              std::vector<FaceRegion>& regions,
                                                              int imageWidth, int imageHeight) const {
    // Add additional vertices for forehead, cheeks, chin to make mesh denser and follow face
    // Use landmarks to interpolate

    if (landmarks.size() < 68) return;

    // Forehead: above eyebrows, 5 points
    HFVec2 leftBrowCenter(0,0), rightBrowCenter(0,0);
    for (int i=22;i<=26;++i) { leftBrowCenter.x+=landmarks[i].x; leftBrowCenter.y+=landmarks[i].y; }
    leftBrowCenter.x/=5; leftBrowCenter.y/=5;
    for (int i=17;i<=21;++i) { rightBrowCenter.x+=landmarks[i].x; rightBrowCenter.y+=landmarks[i].y; }
    rightBrowCenter.x/=5; rightBrowCenter.y/=5;

    float foreheadY = face.y + face.h*0.05f;
    // 5 forehead points
    for (int i=0;i<5;++i) {
        float t = (float)i/4.0f;
        float x = face.x + face.w*(0.2f + t*0.6f);
        float y = foreheadY + (t-0.5f)*(t-0.5f)*face.h*0.05f;
        vertices.emplace_back(x, y, 1.0f);
        uvs.emplace_back(x/(float)imageWidth, y/(float)imageHeight);
        regions.push_back(FaceRegion::FOREHEAD);
    }

    // Cheeks: left and right cheek centers
    HFVec2 leftEyeCenter(0,0), rightEyeCenter(0,0);
    for (int i=42;i<=47;++i) { leftEyeCenter.x+=landmarks[i].x; leftEyeCenter.y+=landmarks[i].y; }
    leftEyeCenter.x/=6; leftEyeCenter.y/=6;
    for (int i=36;i<=41;++i) { rightEyeCenter.x+=landmarks[i].x; rightEyeCenter.y+=landmarks[i].y; }
    rightEyeCenter.x/=6; rightEyeCenter.y/=6;

    // Left cheek: between left eye and jaw
    {
        float x = leftEyeCenter.x + (face.x+face.w*0.75f - leftEyeCenter.x)*0.3f;
        float y = leftEyeCenter.y + (face.y+face.h*0.65f - leftEyeCenter.y)*0.5f;
        vertices.emplace_back(x, y, 0.5f);
        uvs.emplace_back(x/(float)imageWidth, y/(float)imageHeight);
        regions.push_back(FaceRegion::LEFT_CHEEK);
    }
    // Right cheek
    {
        float x = rightEyeCenter.x + (face.x+face.w*0.25f - rightEyeCenter.x)*0.3f;
        float y = rightEyeCenter.y + (face.y+face.h*0.65f - rightEyeCenter.y)*0.5f;
        vertices.emplace_back(x, y, 0.5f);
        uvs.emplace_back(x/(float)imageWidth, y/(float)imageHeight);
        regions.push_back(FaceRegion::RIGHT_CHEEK);
    }

    // Chin center
    {
        float x = (landmarks[8].x + face.x+face.w*0.5f)*0.5f;
        float y = landmarks[8].y + face.h*0.03f;
        vertices.emplace_back(x, y, -1.0f);
        uvs.emplace_back(x/(float)imageWidth, y/(float)imageHeight);
        regions.push_back(FaceRegion::CHIN);
    }

    // Additional nose bridge points for better mesh
    {
        float x = (leftEyeCenter.x + rightEyeCenter.x)*0.5f;
        float y = (leftEyeCenter.y + rightEyeCenter.y)*0.5f + face.h*0.05f;
        vertices.emplace_back(x, y, 6.0f);
        uvs.emplace_back(x/(float)imageWidth, y/(float)imageHeight);
        regions.push_back(FaceRegion::NOSE_BRIDGE);
    }
}

void ProductionFaceMeshGenerator::GenerateIndices(const std::vector<HFVec2>& landmarks,
                                                   int vertexCount,
                                                   std::vector<int>& indices) const {
    // Generate indices for face mesh using landmark topology
    // We have 68 landmarks + additional vertices (forehead 5, cheeks 2, chin 1, nose 1 = 9) = 77 total
    // Create triangles that follow face shape

    indices.clear();
    indices.reserve(300); // estimate

    // Jaw to nose and eyes: create fan from nose bridge
    // Use landmark indices directly for face contour

    // Helper lambda to add triangle if indices valid
    auto addTri = [&](int a, int b, int c) {
        if (a>=0 && a<vertexCount && b>=0 && b<vertexCount && c>=0 && c<vertexCount) {
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);
        }
    };

    // Jaw chain (0-16) connected to nose and cheeks
    // Create triangles: jaw points to nose tip and chin
    for (int i=0;i<16;++i) {
        int next = i+1;
        // Connect jaw to nose bridge and to chin region
        // Triangle jaw_i, jaw_next, nose_bridge (27)
        addTri(i, next, 27);
        // Triangle jaw_i, nose_bridge, nose_tip (30)
        addTri(i, 27, 30);
    }

    // Right eyebrow to right eye
    for (int i=0;i<4;++i) {
        int brow = 17+i;
        int browNext = 17+i+1;
        int eye = 36 + i; // approximate mapping
        int eyeNext = 36 + i+1;
        if (eyeNext > 41) eyeNext = 36;
        addTri(brow, browNext, eye);
        addTri(browNext, eyeNext, eye);
    }

    // Left eyebrow to left eye
    for (int i=0;i<4;++i) {
        int brow = 22+i;
        int browNext = 22+i+1;
        int eye = 42 + i;
        int eyeNext = 42 + i+1;
        if (eyeNext > 47) eyeNext = 42;
        addTri(brow, browNext, eye);
        addTri(browNext, eyeNext, eye);
    }

    // Nose bridge to nose tip fan
    for (int i=27;i<29;++i) {
        addTri(i, i+1, 30);
        addTri(i, 30, 31);
    }
    // Last bridge segment 29-30-31
    addTri(29, 30, 31);
    // Nose tip to outer lip
    addTri(31, 33, 48);
    addTri(33, 51, 48);
    addTri(33, 54, 51);

    // Right eye fan (36-41 around center)
    {
        int center = 36; // use first eye point as center approximation
        for (int i=37;i<41;++i) {
            addTri(center, i, i+1);
        }
        addTri(center, 41, 37);
    }

    // Left eye fan
    {
        int center = 42;
        for (int i=43;i<47;++i) {
            addTri(center, i, i+1);
        }
        addTri(center, 47, 43);
    }

    // Outer lip fan (48-59) - use 48 as center to avoid degenerate, but ensure distinct
    {
        // Use 48 as center for outer lip fan, perimeter 49-59 + 48 closing
        for (int i=49;i<59;++i) {
            addTri(48, i, i+1);
        }
        addTri(48, 59, 49);
        // Also connect outer lip to inner lip for closed mouth
        for (int i=48;i<59;++i) {
            int inner = 60 + (i-48) % 8;
            int innerNext = 60 + (i+1-48) % 8;
            if (inner < vertexCount && innerNext < vertexCount) {
                addTri(i, inner, innerNext);
            }
        }
    }

    // Inner lip fan (60-67) - use 60 as center, perimeter 61-67
    {
        for (int i=61;i<67;++i) {
            addTri(60, i, i+1);
        }
        addTri(60, 67, 61);
    }

    // Connect forehead additional vertices (68-72) to eyebrows and eyes
    // Forehead vertices are at indices 68..72 (after 68 landmarks)
    if (vertexCount > 68) {
        int foreheadStart = 68;
        // Forehead to eyebrows
        for (int i=0;i<4;++i) {
            if (foreheadStart+i < vertexCount && foreheadStart+i+1 < vertexCount) {
                addTri(foreheadStart+i, foreheadStart+i+1, 19); // right brow center
                addTri(foreheadStart+i, 19, 24); // left brow center
            }
        }
        // Cheeks
        if (vertexCount > 73) {
            int leftCheek = 73;
            int rightCheek = 74;
            if (leftCheek < vertexCount) {
                addTri(leftCheek, 42, 48); // left eye to outer lip
                addTri(leftCheek, 48, 8); // to jaw
            }
            if (rightCheek < vertexCount) {
                addTri(rightCheek, 36, 48);
                addTri(rightCheek, 48, 8);
            }
        }
        // Chin
        if (vertexCount > 75) {
            int chin = 75;
            if (chin < vertexCount) {
                addTri(chin, 6, 8);
                addTri(chin, 8, 10);
            }
        }
        // Nose bridge additional
        if (vertexCount > 76) {
            int noseAdd = 76;
            if (noseAdd < vertexCount) {
                addTri(noseAdd, 27, 28);
                addTri(noseAdd, 28, 29);
            }
        }
    }

    // Ensure we have at least some triangles
    if (indices.empty()) {
        // Fallback: simple fan from first landmark
        for (int i=1;i< (int)std::min((size_t)vertexCount-1, (size_t)67); ++i) {
            addTri(0, i, i+1);
        }
    }
}

HFResult ProductionFaceMeshGenerator::Generate(const std::vector<HFVec2>& landmarks,
                                                const std::vector<HFVec3>& landmarks3D,
                                                const FaceDetection& face,
                                                int imageWidth, int imageHeight,
                                                HFFaceMesh& outMesh) {
    if (!initialized) return HF_RESULT_NOT_INITIALIZED;
    if (landmarks.empty() || landmarks3D.empty()) return HF_RESULT_INVALID_PARAM;
    if (landmarks.size() != landmarks3D.size()) return HF_RESULT_INVALID_PARAM;

    outMesh.Clear();
    outMesh.width = imageWidth;
    outMesh.height = imageHeight;

    // Vertices from landmarks
    outMesh.vertices.reserve(landmarks.size() + 9);
    outMesh.uv.reserve(landmarks.size() + 9);
    outMesh.vertexRegions.reserve(landmarks.size() + 9);

    for (size_t i=0; i<landmarks.size(); ++i) {
        HFVec3 v(landmarks[i].x, landmarks[i].y, landmarks3D[i].z);
        outMesh.vertices.push_back(v);
        HFVec2UV uv(landmarks[i].x / (float)imageWidth, landmarks[i].y / (float)imageHeight);
        outMesh.uv.push_back(uv);
        outMesh.vertexRegions.push_back(GetRegionForLandmark((int)i));
    }

    // Additional vertices for forehead, cheeks, chin, nose
    GenerateAdditionalVertices(landmarks, face, outMesh.vertices, outMesh.uv, outMesh.vertexRegions, imageWidth, imageHeight);

    // Generate indices
    GenerateIndices(landmarks, (int)outMesh.vertices.size(), outMesh.indices);

    if (!outMesh.IsValid()) {
        return HF_RESULT_FAIL;
    }

    return HF_RESULT_OK;
}

} // namespace huanface
