/**
 * HuanFace Render Backend Factory — Phase 3
 */

#include "render_backend.h"
#include "null_backend.h"

#ifdef _WIN32
// D3D11 backend only on Windows
// Forward declare, implementation in d3d11/d3d11_backend.cpp
namespace huanface {
std::unique_ptr<IRenderBackend> CreateD3D11Backend();
}
#endif

namespace huanface {

std::unique_ptr<IRenderBackend> CreateNullBackend() {
    return std::make_unique<NullRenderBackend>();
}

std::unique_ptr<IRenderBackend> CreateOpenGLBackend() {
    // P1 stub — for Phase 3 return null backend with NOT_SUPPORTED behavior
    // In real impl, would return OpenGLBackend
    return std::make_unique<NullRenderBackend>();
}

std::unique_ptr<IRenderBackend> CreateRenderBackend(HFRenderBackendType type) {
    switch (type) {
        case HF_RENDER_BACKEND_D3D11:
#ifdef _WIN32
            return CreateD3D11Backend();
#else
            // On non-Windows, D3D11 not available, fallback to null
            return CreateNullBackend();
#endif
        case HF_RENDER_BACKEND_OPENGL:
            return CreateOpenGLBackend();
        case HF_RENDER_BACKEND_AUTO:
        default:
#ifdef _WIN32
            // Try D3D11 first
            {
                auto backend = CreateD3D11Backend();
                if (backend) {
                    HFResult res = backend->Init(nullptr);
                    if (res == HF_RESULT_OK) {
                        return backend;
                    }
                }
            }
#endif
            // Fallback to null (software) for testing
            return CreateNullBackend();
    }
}

} // namespace huanface
