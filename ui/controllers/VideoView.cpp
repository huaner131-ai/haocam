#include "ui/controllers/VideoView.h"

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <atomic>
#include <rhi/qrhi.h>

#include "core/logging/Logger.h"
#include "ui/controllers/EngineController.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include "graphics/compositor/Compositor.h"
#endif

namespace haocam::app {

#ifdef Q_OS_WIN
// Cross-device imports stay alive while the QSGTexture wrapping them lives.
inline void releaseImportKeepalive(void* keepalive) {
    if (keepalive) static_cast<ID3D11Texture2D*>(keepalive)->Release();
}
#else
inline void releaseImportKeepalive(void*) {}
#endif

namespace {
constexpr const char* kCategory = "preview";
}

VideoView::VideoView(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void VideoView::releaseResources() {
    // Called on the render thread: safe place to drop QSGTextures.
    for (auto& [native, import] : m_imports) {
        delete import.texture;
        releaseImportKeepalive(import.keepalive);
    }
    m_imports.clear();
    m_renderDevice = nullptr;
    m_attachAttempted = false; // allow re-attach if the item re-enters a window
}

VideoView::~VideoView() {
    // QSGTextures must be released on the render thread; window() is already
    // gone or the node was cleaned up via releaseResources(). Deleting here
    // is the last resort during teardown.
    for (auto& [native, import] : m_imports) {
        delete import.texture;
        releaseImportKeepalive(import.keepalive);
    }
    m_imports.clear();
}

#ifdef Q_OS_WIN

QSGNode* VideoView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setOwnsTexture(false);
    }

    // Freeze triage: proves the scene-graph render thread keeps syncing and
    // shows whether the compositor ever produced output. If these lines stop
    // while the ui-heartbeat continues, the render thread is wedged (GUI
    // blocks at the next sync -> "Not Responding").
    {
        static std::atomic<uint64_t> s_syncCount{0};
        const uint64_t syncNo = s_syncCount.fetch_add(1) + 1;
        if (syncNo <= 3 || (syncNo % 300) == 0) {
            auto* comp = EngineController::sharedCompositor();
            HAOCAM_LOG_INFO(kCategory, "VideoView sync #{} (compositor={} latestFrame={})",
                            syncNo, comp ? "ready" : "null",
                            comp ? comp->latestFrameId() : 0);
        }
    }

    // ---- First frame: fetch the scene graph D3D11 device and start ----
    if (!m_attachAttempted) {
        m_attachAttempted = true;
        QSGRendererInterface* rif = window() ? window()->rendererInterface() : nullptr;
        if (!rif) {
            emit previewFailed("Qt Quick renderer interface unavailable");
            return node;
        }
        if (rif->graphicsApi() != QSGRendererInterface::Direct3D11) {
            emit previewFailed("Qt Quick is not using Direct3D 11 (set QSG_RHI_BACKEND=d3d11)");
            return node;
        }
        auto* device = static_cast<ID3D11Device*>(
            rif->getResource(window(), QSGRendererInterface::DeviceResource));
        auto* context = static_cast<ID3D11DeviceContext*>(
            rif->getResource(window(), QSGRendererInterface::DeviceContextResource));
        m_renderDevice = device;
        if (!device || !context) {
            emit previewFailed("Direct3D 11 device unavailable from Qt Quick");
            return node;
        }
        if (!EngineController::attachToPipeline(window(), device, context)) {
            emit previewFailed("Engine failed to initialize on the Qt Quick device");
            return node;
        }
    }

    auto* compositor = EngineController::sharedCompositor();
    if (!compositor || !compositor->valid()) {
        return node;
    }

    // ---- Display the newest finished frame ----
    const uint64_t latest = compositor->latestFrameId();
    if (latest != m_displayedFrameId && latest != 0) {
        const CompositorOutput output = compositor->latestOutput();
        if (output.texture && output.texture->native()) {
            m_displayedFrameId = output.frameId;

            void* native = output.texture->native();
            auto it = m_imports.find(native);
            if (it == m_imports.end()) {
                // The engine owns its own D3D11 device; its output textures are
                // created with D3D11_RESOURCE_MISC_SHARED. Open the shared
                // resource on the RENDER device and wrap that view into the
                // QRhi texture. Fallback: wrap the native pointer directly
                // (compositor running on the render device).
                void* importNative = native;
                void* keepalive = nullptr;
                Microsoft::WRL::ComPtr<ID3D11Texture2D> engineTex(
                    static_cast<ID3D11Texture2D*>(native));
                Microsoft::WRL::ComPtr<IDXGIResource> dxgiResource;
                HANDLE sharedHandle = nullptr;
                static bool s_sharedPathLogged = false;
                if (m_renderDevice && SUCCEEDED(engineTex.As(&dxgiResource)) &&
                    SUCCEEDED(dxgiResource->GetSharedHandle(&sharedHandle)) &&
                    sharedHandle) {
                    Microsoft::WRL::ComPtr<ID3D11Texture2D> view;
                    if (SUCCEEDED(static_cast<ID3D11Device*>(m_renderDevice)
                                      ->OpenSharedResource(sharedHandle,
                                                           __uuidof(ID3D11Texture2D),
                                                           &view))) {
                        importNative = view.Get();
                        keepalive = view.Detach(); // released with the import
                        if (!s_sharedPathLogged) {
                            s_sharedPathLogged = true;
                            HAOCAM_LOG_INFO(kCategory, "Importing engine frames via "
                                                       "cross-device shared resource");
                        }
                    }
                }
                if (!keepalive) {
                    // Producer texture lives on THIS device (pool keeps it
                    // alive) - wrap it directly; our extra AddRef from the
                    // ComPtr above is released at scope exit.
                    if (!s_sharedPathLogged) {
                        s_sharedPathLogged = true;
                        HAOCAM_LOG_WARN(kCategory, "Shared-resource import unavailable; "
                                                   "wrapping the texture directly");
                    }
                }

                QRhi* rhi = static_cast<QRhi*>(window()->rendererInterface()->getResource(
                    window(), QSGRendererInterface::RhiResource));
                if (rhi) {
                    QRhiTexture* rhiTexture =
                        rhi->newTexture(QRhiTexture::BGRA8,
                                        QSize(static_cast<int>(output.width),
                                              static_cast<int>(output.height)),
                                        1, QRhiTexture::Flags());
                    QRhiTexture::NativeTexture nativeDesc;
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
                    // Qt 6.9 changed NativeTexture::object from void* to a
                    // 64-bit integer handle.
                    nativeDesc.object = reinterpret_cast<quint64>(importNative);
#else
                    nativeDesc.object = importNative;
#endif
                    nativeDesc.layout = 0;
                    if (rhiTexture && rhiTexture->createFrom(nativeDesc)) {
                        QSGTexture* qsgTexture = window()->createTextureFromRhiTexture(rhiTexture);
                        if (qsgTexture) {
                            it = m_imports
                                     .emplace(native,
                                              ImportedTexture{native, output.frameId,
                                                              qsgTexture, keepalive})
                                     .first;
                        }
                    } else {
                        delete rhiTexture;
                        releaseImportKeepalive(keepalive);
                    }
                }
                if (it == m_imports.end()) {
                    emit previewFailed("Failed to import the final texture into Qt Quick");
                    return node;
                }
            }

            node->setTexture(it->second.texture);
            node->setFiltering(QSGTexture::Linear);
            node->setRect(boundingRect());
            node->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);
            compositor->notifyOutputConsumed(output.frameId);
        }
    }
    update(); // keep presenting new frames

    return node;
}

#else // !Q_OS_WIN

QSGNode* VideoView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setOwnsTexture(false);
    }
    return node;
}

#endif

} // namespace haocam::app
