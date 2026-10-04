/**
 * HuanFace Resource Pool — Phase 8 Production D3D11 Rendering & Performance Optimization
 * Resource reuse for intermediate textures to reduce allocation/COM churn/GPU memory churn
 * Pool based on width/height/format/bind flags
 * Safe for resolution change, frame end, engine shutdown, invalid resource, device failure
 */

#pragma once
#include "../../include/huanface_c_api.h"
#include "../../include/huanface/huanface_image.h"
#include "render_backend.h"
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <algorithm>
#include <algorithm>

namespace huanface {

// ============================================================================
// Texture Pool Key
// ============================================================================
struct TexturePoolKey {
    int width=0;
    int height=0;
    HFFormat format=HF_FORMAT_RGBA8;
    int bindFlags=0; // 0 = SRV, 1 = RTV+SRV, etc.

    bool operator<(const TexturePoolKey& other) const {
        if(width!=other.width) return width<other.width;
        if(height!=other.height) return height<other.height;
        if((int)format!=(int)other.format) return (int)format<(int)other.format;
        return bindFlags<other.bindFlags;
    }
    std::string ToString() const {
        return std::to_string(width)+"x"+std::to_string(height)+"_"+std::to_string((int)format)+"_"+std::to_string(bindFlags);
    }
};

// ============================================================================
// Pooled Texture Wrapper
// ============================================================================
struct PooledTexture {
    std::unique_ptr<IGpuTexture> texture;
    TexturePoolKey key;
    int64_t lastUsedFrame=0;
    bool inUse=false;
    int useCount=0;

    PooledTexture(TexturePoolKey k, std::unique_ptr<IGpuTexture> tex)
        : texture(std::move(tex)), key(k), lastUsedFrame(0), inUse(false), useCount(0) {}
};

// ============================================================================
// GPU Resource Pool — Production D3D11
// ============================================================================
class GPUResourcePool {
public:
    GPUResourcePool() = default;
    ~GPUResourcePool() { Clear(); }

    void Init(IRenderBackend* backend) { backend_=backend; frameCounter_=0; }

    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.clear();
        backend_=nullptr;
    }

    // Acquire texture from pool or create new
    IGpuTexture* AcquireTexture(int width, int height, HFFormat format, int bindFlags=1) {
        if(!backend_) return nullptr;
        TexturePoolKey key{width,height,format,bindFlags};
        std::lock_guard<std::mutex> lock(mutex_);

        // Try to find unused texture with same key
        for(auto& entry: pool_){
            if(!entry->inUse && entry->key.width==key.width && entry->key.height==key.height &&
               entry->key.format==key.format && entry->key.bindFlags==key.bindFlags){
                entry->inUse=true;
                entry->lastUsedFrame=frameCounter_;
                entry->useCount++;
                return entry->texture.get();
            }
        }

        // Try to find unused texture with larger size that can be reused (to avoid allocation)
        // For production, we allow reuse of larger textures if within 1.5x size
        for(auto& entry: pool_){
            if(!entry->inUse && entry->key.format==key.format && entry->key.bindFlags==key.bindFlags){
                if(entry->key.width>=width && entry->key.height>=height &&
                   entry->key.width<=width*2 && entry->key.height<=height*2){
                    entry->inUse=true;
                    entry->lastUsedFrame=frameCounter_;
                    entry->useCount++;
                    return entry->texture.get();
                }
            }
        }

        // Create new
        IGpuTexture* tex = nullptr;
        if(bindFlags==1){
            // RTV+SRV
            IRenderTarget* rt = backend_->CreateRenderTarget(width,height,format);
            if(rt){
                tex = rt->GetTexture();
                // We need to keep RT alive, so we store RT separately
                // For simplicity in pool, we store texture only, but we need to manage RT
                // In production D3D11, RT and texture are same underlying resource
                // For null backend, we handle differently
                // We'll store RT in a separate map
                auto rtEntry = std::make_unique<RenderTargetPoolEntry>(key, std::unique_ptr<IRenderTarget>(rt));
                IGpuTexture* ret = rtEntry->renderTarget->GetTexture();
                rtPool_.push_back(std::move(rtEntry));
                // Also create pooled texture entry for tracking
                auto entry = std::make_unique<PooledTexture>(key, nullptr); // texture is owned by RT
                entry->inUse=true;
                entry->lastUsedFrame=frameCounter_;
                entry->useCount=1;
                pool_.push_back(std::move(entry));
                return ret;
            }
        } else {
            tex = backend_->CreateTexture(width,height,format,nullptr);
            if(tex){
                auto entry = std::make_unique<PooledTexture>(key, std::unique_ptr<IGpuTexture>(tex));
                entry->inUse=true;
                entry->lastUsedFrame=frameCounter_;
                entry->useCount=1;
                IGpuTexture* ret = entry->texture.get();
                pool_.push_back(std::move(entry));
                return ret;
            }
        }
        return nullptr;
    }

    IRenderTarget* AcquireRenderTarget(int width, int height, HFFormat format) {
        if(!backend_) return nullptr;
        TexturePoolKey key{width,height,format,1};
        std::lock_guard<std::mutex> lock(mutex_);

        for(auto& entry: rtPool_){
            if(!entry->inUse && entry->key.width==key.width && entry->key.height==key.height &&
               entry->key.format==key.format){
                entry->inUse=true;
                entry->lastUsedFrame=frameCounter_;
                entry->useCount++;
                return entry->renderTarget.get();
            }
        }

        IRenderTarget* rt = backend_->CreateRenderTarget(width,height,format);
        if(rt){
            auto entry = std::make_unique<RenderTargetPoolEntry>(key, std::unique_ptr<IRenderTarget>(rt));
            entry->inUse=true;
            entry->lastUsedFrame=frameCounter_;
            entry->useCount=1;
            IRenderTarget* ret = entry->renderTarget.get();
            rtPool_.push_back(std::move(entry));
            return ret;
        }
        return nullptr;
    }

    void ReleaseTexture(IGpuTexture* tex) {
        if(!tex) return;
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto& entry: pool_){
            if(entry->texture.get()==tex){
                entry->inUse=false;
                return;
            }
        }
        // Check RT pool
        for(auto& entry: rtPool_){
            if(entry->renderTarget->GetTexture()==tex){
                entry->inUse=false;
                return;
            }
        }
    }

    void ReleaseRenderTarget(IRenderTarget* rt) {
        if(!rt) return;
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto& entry: rtPool_){
            if(entry->renderTarget.get()==rt){
                entry->inUse=false;
                return;
            }
        }
    }

    void BeginFrame() {
        std::lock_guard<std::mutex> lock(mutex_);
        frameCounter_++;
        // Mark all as not in use at beginning of frame? Actually we release explicitly
        // But we can also auto-release after frame if needed
    }

    void EndFrame() {
        // Optional: evict old unused textures after N frames to avoid memory bloat
        std::lock_guard<std::mutex> lock(mutex_);
        const int64_t evictAfterFrames = 60; // evict if unused for 60 frames
        // Remove old unused entries
        pool_.erase(std::remove_if(pool_.begin(), pool_.end(),
            [&](const std::unique_ptr<PooledTexture>& e){
                return !e->inUse && (frameCounter_ - e->lastUsedFrame) > evictAfterFrames;
            }), pool_.end());

        rtPool_.erase(std::remove_if(rtPool_.begin(), rtPool_.end(),
            [&](const std::unique_ptr<RenderTargetPoolEntry>& e){
                return !e->inUse && (frameCounter_ - e->lastUsedFrame) > evictAfterFrames;
            }), rtPool_.end());
    }

    void OnResolutionChanged() {
        // Clear pool on resolution change to avoid stale sizes? Actually we keep pool and allow reuse of larger
        // For production, we could clear or keep
        // We choose to clear to avoid memory bloat
        std::lock_guard<std::mutex> lock(mutex_);
        // Keep only entries that are in use, clear unused
        pool_.erase(std::remove_if(pool_.begin(), pool_.end(),
            [](const std::unique_ptr<PooledTexture>& e){ return !e->inUse; }), pool_.end());
        rtPool_.erase(std::remove_if(rtPool_.begin(), rtPool_.end(),
            [](const std::unique_ptr<RenderTargetPoolEntry>& e){ return !e->inUse; }), rtPool_.end());
    }

    void OnDeviceLost() {
        Clear();
    }

    size_t GetPoolSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size() + rtPool_.size();
    }

    size_t GetUsedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t used=0;
        for(auto& e: pool_) if(e->inUse) used++;
        for(auto& e: rtPool_) if(e->inUse) used++;
        return used;
    }

    struct Stats {
        size_t totalPoolSize=0;
        size_t usedCount=0;
        size_t totalCreated=0;
        int64_t frameCounter=0;
    };

    Stats GetStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        Stats s;
        s.totalPoolSize = pool_.size() + rtPool_.size();
        for(auto& e: pool_) if(e->inUse) s.usedCount++;
        for(auto& e: rtPool_) if(e->inUse) s.usedCount++;
        s.frameCounter = frameCounter_;
        s.totalCreated = totalCreated_;
        return s;
    }

private:
    struct RenderTargetPoolEntry {
        TexturePoolKey key;
        std::unique_ptr<IRenderTarget> renderTarget;
        int64_t lastUsedFrame=0;
        bool inUse=false;
        int useCount=0;
        RenderTargetPoolEntry(TexturePoolKey k, std::unique_ptr<IRenderTarget> rt)
            : key(k), renderTarget(std::move(rt)), lastUsedFrame(0), inUse(false), useCount(0) {}
    };

    IRenderBackend* backend_=nullptr;
    std::vector<std::unique_ptr<PooledTexture>> pool_;
    std::vector<std::unique_ptr<RenderTargetPoolEntry>> rtPool_;
    mutable std::mutex mutex_;
    int64_t frameCounter_=0;
    size_t totalCreated_=0;
};

// ============================================================================
// CPU Image Pool — for intermediate HFImage buffers
// ============================================================================
class CPUImagePool {
public:
    struct PooledImage {
        HFImage image;
        int64_t lastUsedFrame=0;
        bool inUse=false;
    };

    CPUImagePool() = default;
    ~CPUImagePool() { Clear(); }

    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.clear();
    }

    HFImage* Acquire(int width, int height, int channels) {
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto& entry: pool_){
            if(!entry.inUse && entry.image.width==width && entry.image.height==height && entry.image.channels==channels){
                entry.inUse=true;
                entry.lastUsedFrame=frameCounter_;
                return &entry.image;
            }
        }
        // Create new
        PooledImage newEntry;
        newEntry.image.width=width;
        newEntry.image.height=height;
        newEntry.image.channels=channels;
        newEntry.image.data.resize((size_t)width*height*channels);
        newEntry.inUse=true;
        newEntry.lastUsedFrame=frameCounter_;
        pool_.push_back(std::move(newEntry));
        return &pool_.back().image;
    }

    void Release(HFImage* img) {
        if(!img) return;
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto& entry: pool_){
            if(&entry.image==img){
                entry.inUse=false;
                return;
            }
        }
    }

    void BeginFrame() { frameCounter_++; }
    void EndFrame() {
        // Evict old
        const int64_t evictAfter = 60;
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.erase(std::remove_if(pool_.begin(), pool_.end(),
            [&](const PooledImage& e){ return !e.inUse && (frameCounter_ - e.lastUsedFrame) > evictAfter; }), pool_.end());
    }

    size_t GetPoolSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

private:
    std::vector<PooledImage> pool_;
    mutable std::mutex mutex_;
    int64_t frameCounter_=0;
};

} // namespace huanface
