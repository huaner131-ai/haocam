/**
 * HuanFace Resource Manager Implementation — Phase 3
 */

#include "resource_manager.h"
#include <algorithm>

namespace huanface {

void ResourceManager::Clear() {
    std::lock_guard<std::mutex> lock(mutex);
    cache.clear();
    totalCacheSize = 0;
}

ResourceType ResourceManager::DetectTypeFromPath(const std::string& path) {
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("textures/") != std::string::npos || lower.find(".png") != std::string::npos || lower.find(".jpg") != std::string::npos) {
        return ResourceType::Texture;
    }
    if (lower.find("shaders/") != std::string::npos || lower.find(".glsl") != std::string::npos || lower.find(".hlsl") != std::string::npos) {
        return ResourceType::Shader;
    }
    if (lower.find("meshes/") != std::string::npos || lower.find(".obj") != std::string::npos || lower.find(".json") != std::string::npos) {
        // Could be mesh or metadata, check path
        if (lower.find("meshes/") != std::string::npos) return ResourceType::Mesh;
        return ResourceType::Metadata;
    }
    if (lower.find("masks/") != std::string::npos) {
        return ResourceType::Mask;
    }
    if (lower.find("metadata/") != std::string::npos) {
        return ResourceType::Metadata;
    }
    return ResourceType::Unknown;
}

bool ResourceManager::LoadFromBundle(BundleReader& reader, const std::string& path, ResourceType type, std::string& outError) {
    if (path.empty()) {
        outError = "Empty resource path";
        return false;
    }

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = cache.find(path);
        if (it != cache.end() && it->second->loaded) {
            return true; // already cached
        }
    }

    std::vector<uint8_t> data;
    if (!reader.ReadFile(path, data, outError)) {
        return false;
    }

    auto res = std::make_shared<Resource>();
    res->type = type;
    if (res->type == ResourceType::Unknown) {
        res->type = DetectTypeFromPath(path);
    }
    res->path = path;
    // Extract name from path
    size_t slash = path.find_last_of("/\\");
    res->name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    res->data = std::move(data);
    res->size = res->data.size();
    res->loaded = true;

    {
        std::lock_guard<std::mutex> lock(mutex);
        // If already exists, subtract old size
        auto it = cache.find(path);
        if (it != cache.end()) {
            totalCacheSize -= it->second->size;
        }
        cache[path] = res;
        totalCacheSize += res->size;
    }

    return true;
}

std::shared_ptr<Resource> ResourceManager::GetResource(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    return nullptr;
}

bool ResourceManager::HasResource(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex);
    return cache.find(path) != cache.end();
}

size_t ResourceManager::GetCacheSize() const {
    std::lock_guard<std::mutex> lock(mutex);
    return totalCacheSize;
}

size_t ResourceManager::GetResourceCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return cache.size();
}

void ResourceManager::EvictLRU(size_t maxSizeBytes) {
    // Simple: if total size > max, clear all (for Phase 3 minimal)
    // In production, implement LRU
    std::lock_guard<std::mutex> lock(mutex);
    if (totalCacheSize > maxSizeBytes) {
        // For Phase 3, just clear
        cache.clear();
        totalCacheSize = 0;
    }
}

bool ResourceManager::LoadAllFromManifest(BundleReader& reader, const HFManifest& manifest, std::string& outError) {
    // Load textures
    for (const auto& tex : manifest.textures) {
        if (!LoadFromBundle(reader, tex.path, ResourceType::Texture, outError)) {
            // For Phase 3, we allow missing optional resources? But manifest says required, so fail
            return false;
        }
    }
    // Load masks (optional)
    for (const auto& mask : manifest.masks) {
        std::string err;
        LoadFromBundle(reader, mask.path, ResourceType::Mask, err); // optional, ignore error
    }
    // Load shaders
    for (const auto& sh : manifest.shaders) {
        if (!LoadFromBundle(reader, sh.path, ResourceType::Shader, outError)) {
            return false;
        }
    }
    // Load meshes (optional)
    for (const auto& mesh : manifest.meshes) {
        std::string err;
        LoadFromBundle(reader, mesh.path, ResourceType::Mesh, err);
    }
    // Load metadata thumbnail (optional)
    {
        std::string err;
        LoadFromBundle(reader, "metadata/thumbnail.png", ResourceType::Metadata, err);
    }

    return true;
}

} // namespace huanface
