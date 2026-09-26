#include <fstream>
#include <sstream>

#include "core/config/Json.h"
#include "test_main.h"

using haocam::core::JsonValue;

HAOCAM_TEST(json_parse_scalars) {
    std::string error;
    auto value = JsonValue::parse(R"({"a": 1, "b": -2.5, "c": true, "d": null, "e": "x"})", &error);
    HAOCAM_EXPECT(error.empty());
    HAOCAM_EXPECT(value.isObject());
    HAOCAM_EXPECT_EQ(value.at("a").asInt(), 1);
    HAOCAM_EXPECT(value.at("b").asNumber() < -2.4);
    HAOCAM_EXPECT(value.at("b").asNumber() > -2.6);
    HAOCAM_EXPECT(value.at("c").asBool());
    HAOCAM_EXPECT(value.at("d").isNull());
    HAOCAM_EXPECT_EQ(value.at("e").asString(), "x");
}

HAOCAM_TEST(json_parse_nested_and_arrays) {
    std::string error;
    auto value = JsonValue::parse(
        R"({"outer": {"inner": [1, 2, {"deep": "yes"}]}, "list": []})", &error);
    HAOCAM_EXPECT(error.empty());
    HAOCAM_EXPECT_EQ(value.at("outer").at("inner").size(), 3u);
    HAOCAM_EXPECT_EQ(value.at("outer").at("inner").at(2).at("deep").asString(), "yes");
    HAOCAM_EXPECT(value.at("list").isArray());
    HAOCAM_EXPECT_EQ(value.at("list").size(), 0u);
}

HAOCAM_TEST(json_parse_string_escapes) {
    std::string error;
    auto value = JsonValue::parse(R"({"s": "a\"b\\c\n\t Aé😀"})", &error);
    HAOCAM_EXPECT(error.empty());
    const std::string decoded = value.at("s").asString();
    HAOCAM_EXPECT_EQ(decoded, std::string("a\"b\\c\n\t A\xC3\xA9\xF0\x9F\x98\x80"));
}

HAOCAM_TEST(json_parse_error_positions) {
    std::string error;
    auto value = JsonValue::parse(R"({"a": trz})", &error);
    HAOCAM_EXPECT(value.isNull());
    HAOCAM_EXPECT(!error.empty());

    error.clear();
    value = JsonValue::parse(R"({"a": 1,})", &error);
    HAOCAM_EXPECT(value.isNull());
    HAOCAM_EXPECT(!error.empty());

    error.clear();
    value = JsonValue::parse("[1, 2", &error);
    HAOCAM_EXPECT(value.isNull());
    HAOCAM_EXPECT(!error.empty());
}

HAOCAM_TEST(json_serialize_roundtrip) {
    JsonValue root = JsonValue::makeObject();
    JsonValue beauty = JsonValue::makeObject();
    beauty.set("smoothing", JsonValue(0.72));
    beauty.set("whitening", JsonValue(0.15));
    root.set("name", JsonValue(std::string("Soft Idol")));
    root.set("beauty", std::move(beauty));

    const std::string text = root.serialize(2);
    std::string error;
    JsonValue reparsed = JsonValue::parse(text, &error);
    HAOCAM_EXPECT(error.empty());
    HAOCAM_EXPECT_EQ(reparsed.at("name").asString(), "Soft Idol");
    HAOCAM_EXPECT(reparsed.at("beauty").at("smoothing").asNumber() > 0.719);
}

HAOCAM_TEST(json_preset_sample_file) {
    // Parses the portable preset sample shipped with the repository.
    std::ifstream in(std::string(HAOCAM_SOURCE_DIR) + "/assets/presets/soft_idol.json",
                     std::ios::binary);
    HAOCAM_EXPECT(in.good());
    if (!in.good()) return;
    std::ostringstream buffer;
    buffer << in.rdbuf();

    std::string error;
    JsonValue preset = JsonValue::parse(buffer.str(), &error);
    HAOCAM_EXPECT(error.empty());
    HAOCAM_EXPECT_EQ(preset.at("name").asString(), "Soft Idol");
    HAOCAM_EXPECT(preset.at("beauty").at("smoothing").asNumber() > 0.7);
    HAOCAM_EXPECT(preset.at("makeup").at("lipstick").at("enabled").asBool());
    HAOCAM_EXPECT(!preset.at("ar").at("enabled").asBool());
}
