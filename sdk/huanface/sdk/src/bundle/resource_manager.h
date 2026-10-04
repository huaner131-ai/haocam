/**
 * HuanFace Resource Manager — Phase 3
 * Cache for textures, shaders, meshes, masks
 * Bundle -> ResourceManager -> CPU/GPU resources
 */

#pragma once
#include "bundle_reader.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>

namespace huanface {

enum class ResourceType {
    Texture,
    Shader,
    Mesh,
    Mask,
    Metadata,
    Unknown
};

struct Resource {
    ResourceType type = ResourceType::Unknown;
    std::string name;
    std::string path; // logical path inside bundle
    std::vector<uint8_t> data; // CPU data
    size_t size = 0;
    bool loaded = false;
    // GPU handle placeholder (void* for now, in real impl IGpuTexture* etc.)
    void* gpuHandle = nullptr;
};

class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager() { Clear(); }

    void Clear();

    // Load resource from bundle reader
    bool LoadFromBundle(BundleReader& reader, const std::string& path, ResourceType type, std::string& outError);
    
    // Get resource (cached)
    std::shared_ptr<Resource> GetResource(const std::string& path);
    bool HasResource(const std::string& path) const;

    // Cache management
    size_t GetCacheSize() const;
    size_t GetResourceCount() const;
    void EvictLRU(size_t maxSizeBytes); // simple eviction

    // For testing: load all resources from manifest
    bool LoadAllFromManifest(BundleReader& reader, const HFManifest& manifest, std::string& outError);

private:
    mutable std::mutex mutex;
    std::map<std::string, std::shared_ptr<Resource>> cache;
    size_t totalCacheSize = 0;

    ResourceType DetectTypeFromPath(const std::string& path);
};

} // namespace huanface
