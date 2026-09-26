#pragma once

// QQuickItem that displays the engine's FINAL composited D3D11 texture.
//
// Design (docs/GPU_PIPELINE.md):
//   * The Qt Quick scene graph runs on Direct3D 11 (Windows default RHI).
//   * On the first updatePaintNode(), the Qt Quick ID3D11Device is fetched
//     through QSGRendererInterface and handed to EngineController, so the
//     whole pipeline (capture + effects + composite) runs on that device.
//   * The compositor's final texture is imported into a QRhiTexture via
//     QRhiTexture::createFrom() (same-device requirement satisfied) and
//     wrapped in a QSGTexture through QQuickWindow::createTextureFromRhiTexture().
//     Qt renders the textured quad - no custom Qt shaders needed.
//
// Non-Windows builds (and non-D3D11 RHI backends) show a status overlay; the
// camera still works where a source exists, but GPU preview is unavailable.

#include <QQuickItem>
#include <QSGSimpleTextureNode>

#include <memory>
#include <unordered_map>

namespace haocam::app {

class EngineController;

class VideoView : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(VideoView)

public:
    explicit VideoView(QQuickItem* parent = nullptr);
    ~VideoView() override;

signals:
    void previewFailed(QString reason);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void releaseResources() override;

private:
    void attachEngine();

    EngineController* m_engine = nullptr;
    bool m_attachAttempted = false;
    uint64_t m_displayedFrameId = 0;

    struct ImportedTexture {
        void* native = nullptr;
        uint64_t frameId = 0;
        QSGTexture* texture = nullptr; // owns the wrapping QRhiTexture
    };
    std::unordered_map<void*, ImportedTexture> m_imports;
};

} // namespace haocam::app
