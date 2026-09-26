// Phase 2: beauty contract, provider degradation, JSON persistence, graph
// order (spec sections 14, 16-18, 21, 33).

#include <string>
#include <vector>

#include "effects/EffectContext.h"
#include "effects/EffectGraph.h"
#include "effects/beauty/BeautyProvider.h"
#include "effects/beauty/NullBeautyProvider.h"
#include "test_main.h"

using namespace haocam;

namespace {
class RecordingProvider final : public IBeautyProvider {
public:
    bool initialize(const EffectContext&) override { return false; }
    void shutdown() override {}
    bool isAvailable() const override { return true; }
    std::string statusText() const override { return "Ready"; }
    const char* name() const override { return "Recording"; }
    ProviderType type() const override { return ProviderType::Beauty; }
    Features features() const override { return {}; }

    void setSmoothing(float value) override {
        m_config.smoothing = clamp01(value);
        calls.push_back("smoothing:" + std::to_string(int(m_config.smoothing * 100)));
    }
    void setWhitening(float value) override {
        m_config.whitening = clamp01(value);
        calls.push_back("whitening:" + std::to_string(int(m_config.whitening * 100)));
    }
    void setRosy(float value) override {
        m_config.rosy = clamp01(value);
        calls.push_back("rosy:" + std::to_string(int(m_config.rosy * 100)));
    }
    void setSharpen(float value) override {
        m_config.sharpen = clamp01(value);
        calls.push_back("sharpen:" + std::to_string(int(m_config.sharpen * 100)));
    }
    void setFaceSlim(float value) override {
        m_config.faceSlim = clamp01(value);
        calls.push_back("faceSlim:" + std::to_string(int(m_config.faceSlim * 100)));
    }
    void setEyeSize(float value) override {
        m_config.eyeSize = clamp01(value);
        calls.push_back("eyeSize:" + std::to_string(int(m_config.eyeSize * 100)));
    }
    void setNoseSize(float value) override {
        m_config.noseSize = clamp01(value);
        calls.push_back("noseSize:" + std::to_string(int(m_config.noseSize * 100)));
    }
    void setJawSlim(float value) override {
        m_config.jawSlim = clamp01(value);
        calls.push_back("jawSlim:" + std::to_string(int(m_config.jawSlim * 100)));
    }
    void reset() override {
        m_config.reset();
        calls.push_back("reset");
    }
    BeautyConfig config() const override { return m_config; }
    BeautyResult process(const Frame&, const FaceData&) override { return {}; }

    BeautyConfig m_config;
    std::vector<std::string> calls;
};
} // namespace

HAOCAM_TEST(null_beauty_reports_reason_and_stays_safe) {
    NullBeautyProvider provider("SDK not installed");

    EffectContext context;
    HAOCAM_EXPECT(!provider.initialize(context)); // honest: cannot process
    HAOCAM_EXPECT(!provider.isAvailable());
    HAOCAM_EXPECT_EQ(std::string(provider.statusText()), std::string("SDK not installed"));
    HAOCAM_EXPECT_EQ(std::string(provider.name()), std::string("NullBeauty"));

    // Parameter sets on the null provider must not crash and must clamp.
    provider.setSmoothing(2.0f);
    provider.setJawSlim(-1.0f);
    const BeautyConfig config = provider.config();
    HAOCAM_EXPECT(config.smoothing == 1.0f);
    HAOCAM_EXPECT(config.jawSlim == 0.0f);

    provider.reset();
    HAOCAM_EXPECT(provider.config().smoothing == 0.0f);
    provider.shutdown();
}

HAOCAM_TEST(null_beauty_reason_can_change_after_failed_init) {
    NullBeautyProvider provider("disabled");
    provider.setReason("Initialization failed");
    HAOCAM_EXPECT_EQ(std::string(provider.statusText()),
                     std::string("Initialization failed"));
}

HAOCAM_TEST(beauty_config_defaults_reset_and_equality) {
    BeautyConfig config;
    config.smoothing = 0.7f;
    config.faceSlim = 0.4f;
    HAOCAM_EXPECT(!(config == BeautyConfig{}));

    config.reset();
    HAOCAM_EXPECT(config == BeautyConfig{});

    config.whitening = 1.0f;
    config.clamp();
    HAOCAM_EXPECT(config.whitening == 1.0f);
}

HAOCAM_TEST(beauty_json_round_trip_preserves_parameters) {
    BeautyConfig config;
    config.smoothing = 0.25f;
    config.whitening = 0.5f;
    config.rosy = 0.1f;
    config.sharpen = 0.9f;
    config.faceSlim = 0.33f;
    config.eyeSize = 0.0f;
    config.noseSize = 0.75f;
    config.jawSlim = 1.0f;

    const auto json = beautyConfigToJson(config);
    HAOCAM_EXPECT(json.at("smoothing").asNumber() == 0.25);
    HAOCAM_EXPECT(json.at("jawSlim").asNumber() == 1.0);

    const BeautyConfig parsed = beautyConfigFromJson(json);
    HAOCAM_EXPECT(parsed == config);

    // Malformed JSON -> defaults, no crash.
    const auto bad = haocam::core::JsonValue::parse("not-an-object");
    const BeautyConfig fallback = beautyConfigFromJson(bad);
    HAOCAM_EXPECT(fallback == BeautyConfig{});
}

HAOCAM_TEST(beauty_json_partial_object_fills_defaults) {
    const auto json = haocam::core::JsonValue::parse(R"({"smoothing": 0.4})");
    const BeautyConfig config = beautyConfigFromJson(json);
    HAOCAM_EXPECT(config.smoothing == 0.4f);
    HAOCAM_EXPECT(config.whitening == 0.0f);
    HAOCAM_EXPECT(config.jawSlim == 0.0f);
}

HAOCAM_TEST(beauty_setters_clamp_out_of_range_values) {
    RecordingProvider provider;
    provider.setSmoothing(1.7f);
    provider.setWhitening(-0.3f);
    provider.setSharpen(0.42f);
    provider.setNoseSize(2.0f);
    provider.setEyeSize(-2.0f);
    provider.setRosy(0.5f);
    provider.setFaceSlim(0.5f);
    provider.setJawSlim(1.5f);

    HAOCAM_EXPECT(provider.config().smoothing == 1.0f);
    HAOCAM_EXPECT(provider.config().whitening == 0.0f);
    HAOCAM_EXPECT(provider.config().sharpen == 0.42f);
    HAOCAM_EXPECT(provider.config().noseSize == 1.0f);
    HAOCAM_EXPECT(provider.config().eyeSize == 0.0f);
    HAOCAM_EXPECT(provider.config().jawSlim == 1.0f);
}

HAOCAM_TEST(beauty_reset_clears_every_parameter_and_notifies) {
    RecordingProvider provider;
    provider.setSmoothing(0.9f);
    provider.setFaceSlim(0.8f);
    provider.reset();

    HAOCAM_EXPECT(provider.config().smoothing == 0.0f);
    HAOCAM_EXPECT(provider.config().faceSlim == 0.0f);
    HAOCAM_EXPECT(!provider.calls.empty());
    HAOCAM_EXPECT_EQ(provider.calls.back(), std::string("reset"));
}

HAOCAM_TEST(graph_order_tracking_before_beauty_before_composite) {
    EffectGraph graph;
    const EffectNode* tracking = graph.findStage(EffectStage::FaceTracking);
    const EffectNode* beauty = graph.findStage(EffectStage::Beauty);
    const EffectNode* composite = graph.findStage(EffectStage::FinalComposite);
    HAOCAM_EXPECT(tracking != nullptr);
    HAOCAM_EXPECT(beauty != nullptr);
    HAOCAM_EXPECT(composite != nullptr);

    size_t trackingIndex = 0, beautyIndex = 0, compositeIndex = 0;
    for (size_t i = 0; i < graph.nodeCount(); ++i) {
        if (graph.node(i)->stage() == EffectStage::FaceTracking) trackingIndex = i;
        if (graph.node(i)->stage() == EffectStage::Beauty) beautyIndex = i;
        if (graph.node(i)->stage() == EffectStage::FinalComposite) compositeIndex = i;
    }
    HAOCAM_EXPECT(trackingIndex < beautyIndex);
    HAOCAM_EXPECT(beautyIndex < compositeIndex);
    HAOCAM_EXPECT(graph.isOrderValid());
}

HAOCAM_TEST(graph_active_stage_names_in_canonical_order) {
    EffectGraph graph;
    // Defaults: capture, color conversion, composite, outputs (Phase 1).
    graph.setStageEnabled(EffectStage::FaceTracking, true); // Phase 2 slot
    graph.setStageEnabled(EffectStage::Beauty, true);

    const auto names = graph.activeStageNames();
    HAOCAM_EXPECT_EQ(names.size(), size_t(6));
    HAOCAM_EXPECT_EQ(names[0], std::string("Camera Capture"));
    HAOCAM_EXPECT_EQ(names[2], std::string("Face Tracking"));
    HAOCAM_EXPECT_EQ(names[3], std::string("Beauty"));
    HAOCAM_EXPECT_EQ(names[4], std::string("Final Composite"));
    HAOCAM_EXPECT_EQ(names[5], std::string("Outputs"));
}
