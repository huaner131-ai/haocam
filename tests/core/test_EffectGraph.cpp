#include <algorithm>

#include "effects/EffectGraph.h"
#include "test_main.h"

using haocam::EffectGraph;
using haocam::EffectStage;

HAOCAM_TEST(effectgraph_default_order_matches_spec) {
    EffectGraph graph;
    const auto names = graph.activeStageNames();
    // Phase-1 defaults: capture, color conversion, composite, outputs.
    HAOCAM_EXPECT_EQ(names.size(), 4u);
    HAOCAM_EXPECT_EQ(names.front(), std::string("Camera Capture"));
    HAOCAM_EXPECT_EQ(names[1], std::string("Color Conversion"));
    HAOCAM_EXPECT_EQ(names[names.size() - 2], std::string("Final Composite"));
    HAOCAM_EXPECT_EQ(names.back(), std::string("Outputs"));
    HAOCAM_EXPECT(graph.isOrderValid());
}

HAOCAM_TEST(effectgraph_enable_disable) {
    EffectGraph graph;
    HAOCAM_EXPECT(!graph.isStageEnabled(EffectStage::Beauty));
    graph.setStageEnabled(EffectStage::Beauty, true);
    HAOCAM_EXPECT(graph.isStageEnabled(EffectStage::Beauty));
    const auto names = graph.activeStageNames();
    HAOCAM_EXPECT(std::find(names.begin(), names.end(), std::string("Beauty")) != names.end());
    graph.setStageEnabled(EffectStage::Beauty, false);
    HAOCAM_EXPECT(!graph.isStageEnabled(EffectStage::Beauty));
}

HAOCAM_TEST(effectgraph_nodes_follow_canonical_order) {
    // The canonical order is respected by construction; isOrderValid guards
    // future edits/provider registrations that could shuffle nodes.
    EffectGraph graph;
    HAOCAM_EXPECT(graph.isOrderValid());
    const EffectStage expected[] = {
        EffectStage::CameraCapture,   EffectStage::ColorConversion,
        EffectStage::FaceTracking,    EffectStage::FaceReshape,
        EffectStage::Beauty,          EffectStage::Makeup,
        EffectStage::AR,              EffectStage::ColorLut,
        EffectStage::FinalComposite,  EffectStage::Outputs,
    };
    HAOCAM_EXPECT_EQ(graph.nodeCount(), sizeof(expected) / sizeof(expected[0]));
    for (size_t i = 0; i < graph.nodeCount(); ++i) {
        HAOCAM_EXPECT(graph.node(i)->stage() == expected[i]);
    }
}
