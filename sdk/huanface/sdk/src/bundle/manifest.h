/**
 * HuanFace Bundle Manifest — Phase 3
 * Parses manifest.json from .hfbundle ZIP open format
 * Clean-room, no FaceUnity encrypted bundle handling
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include "json_parser.h"

namespace huanface {

enum class HFParamTypeInternal {
    Float,
    Int,
    Bool,
    Color,
    Vec2,
    Vec3,
    Vec4,
    Texture,
    Enum
};

struct HFTextureDesc {
    std::string name;
    std::string path;
    std::string format; // RGBA8 etc.
    int width = 0;
    int height = 0;
};

struct HFMaskDesc {
    std::string name;
    std::string path;
    std::string type; // r8, rgba8, json
};

struct HFShaderDesc {
    std::string name;
    std::string path;
    std::string type; // glsl, hlsl
    std::string stage; // vertex, fragment, compute
};

struct HFMeshDesc {
    std::string name;
    std::string path;
    std::string format; // json, obj
};

struct HFParameterDesc {
    std::string name;
    std::string type; // float, int, bool, color, vec2, vec3, vec4, texture, enum
    double minValue = 0.0;
    double maxValue = 1.0;
    bool hasMin = false;
    bool hasMax = false;
    bool hasDefault = false;
    JsonValue defaultValue; // can be number, array, string, bool
    std::vector<std::string> options; // for enum
    std::string group;
    std::string description;
};

struct HFPassDesc {
    std::string name;
    std::string shader;
    std::vector<std::string> textures;
    std::vector<std::string> masks;
    std::string blend = "normal";
    int order = 0;
};

struct HFManifest {
    std::string format; // Must be HuanFaceBundle
    std::string version; // e.g., "1.0"
    std::string type; // makeup, beauty, filter, effect, hair, face, combined
    std::string name;
    std::string author;
    std::string description;
    std::string created;
    std::vector<std::string> dependencies;
    std::vector<HFTextureDesc> textures;
    std::vector<HFMaskDesc> masks;
    std::vector<HFShaderDesc> shaders;
    std::vector<HFMeshDesc> meshes;
    std::vector<HFParameterDesc> parameters;
    std::vector<HFPassDesc> passes;
};

class ManifestParser {
public:
    // Parse from JSON string, throws on error
    static HFManifest Parse(const std::string& jsonStr);
    // Validate manifest, returns error message if invalid, empty if ok
    static std::string Validate(const HFManifest& manifest);
    // Helper to check path traversal
    static bool IsPathTraversal(const std::string& path);
};

} // namespace huanface
