/**
 * HuanFace Face Mesh Generator — Phase 5 Production
 * Real face mesh following face shape, not 5x5 grid
 */

#pragma once
#include "face_data.h"
#include "face_detector.h"
#include "../../include/huanface_c_api.h"
#include <vector>

namespace huanface {

class IFaceMeshGenerator {
public:
    virtual ~IFaceMeshGenerator() = default;
    virtual HFResult Init(const HFEngineConfigC& config) = 0;
    virtual void Shutdown() = 0;
    virtual HFResult Generate(const std::vector<HFVec2>& landmarks,
                              const std::vector<HFVec3>& landmarks3D,
                              const FaceDetection& face,
                              int imageWidth, int imageHeight,
                              HFFaceMesh& outMesh) = 0;
    virtual std::string GetName() const = 0;
};

class ProductionFaceMeshGenerator : public IFaceMeshGenerator {
public:
    ProductionFaceMeshGenerator() = default;
    ~ProductionFaceMeshGenerator() override = default;

    HFResult Init(const HFEngineConfigC& config) override;
    void Shutdown() override;
    HFResult Generate(const std::vector<HFVec2>& landmarks,
                      const std::vector<HFVec3>& landmarks3D,
                      const FaceDetection& face,
                      int imageWidth, int imageHeight,
                      HFFaceMesh& outMesh) override;
    std::string GetName() const override { return "ProductionFaceMeshGenerator"; }

private:
    bool initialized = false;

    void GenerateAdditionalVertices(const std::vector<HFVec2>& landmarks,
                                    const FaceDetection& face,
                                    std::vector<HFVec3>& vertices,
                                    std::vector<HFVec2UV>& uvs,
                                    std::vector<FaceRegion>& regions,
                                    int imageWidth, int imageHeight) const;

    void GenerateIndices(const std::vector<HFVec2>& landmarks,
                         int vertexCount,
                         std::vector<int>& indices) const;

    FaceRegion GetRegionForLandmark(int idx) const;
};

} // namespace huanface
