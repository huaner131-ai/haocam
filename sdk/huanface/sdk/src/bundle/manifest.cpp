/**
 * HuanFace Manifest Parser Implementation — Phase 3
 */

#include "manifest.h"
#include <stdexcept>
#include <algorithm>

namespace huanface {

bool ManifestParser::IsPathTraversal(const std::string& path) {
    // Reject dangerous patterns
    if (path.empty()) return false;
    // Check for .. , absolute path, drive letter, etc.
    if (path.find("..") != std::string::npos) return true;
    if (path.find(":") != std::string::npos) {
        // Allow colon only if not Windows drive? Simpler: reject any colon
        // But also reject absolute Unix path starting with /
        // For safety, reject if contains : or starts with / or \
        return true;
    }
    if (!path.empty() && (path[0] == '/' || path[0] == '\\')) return true;
    // Check for backslash traversal
    if (path.find("\\..") != std::string::npos) return true;
    // Normalize and check
    // Also reject if path contains // or is empty segment?
    return false;
}

HFManifest ManifestParser::Parse(const std::string& jsonStr) {
    JsonValue root = JsonParser::Parse(jsonStr);
    if (!root.IsObject()) {
        throw std::runtime_error("Manifest root must be object");
    }

    HFManifest m;

    m.format = root.GetField("format").GetString();
    m.version = root.GetField("version").GetString();
    m.type = root.GetField("type").GetString();
    m.name = root.GetField("name").GetString();
    m.author = root.GetField("author").GetString();
    m.description = root.GetField("description").GetString();
    m.created = root.GetField("created").GetString();

    // dependencies
    const JsonValue& deps = root.GetField("dependencies");
    if (deps.IsArray()) {
        for (const auto& v : deps.arrayValue) {
            if (v.IsString()) m.dependencies.push_back(v.stringValue);
        }
    }

    // textures
    const JsonValue& texs = root.GetField("textures");
    if (texs.IsArray()) {
        for (const auto& v : texs.arrayValue) {
            if (!v.IsObject()) continue;
            HFTextureDesc d;
            d.name = v.GetField("name").GetString();
            d.path = v.GetField("path").GetString();
            d.format = v.GetField("format").GetString("RGBA8");
            d.width = (int)v.GetField("width").GetNumber(0);
            d.height = (int)v.GetField("height").GetNumber(0);
            m.textures.push_back(std::move(d));
        }
    }

    // masks
    const JsonValue& masks = root.GetField("masks");
    if (masks.IsArray()) {
        for (const auto& v : masks.arrayValue) {
            if (!v.IsObject()) continue;
            HFMaskDesc d;
            d.name = v.GetField("name").GetString();
            d.path = v.GetField("path").GetString();
            d.type = v.GetField("type").GetString("r8");
            m.masks.push_back(std::move(d));
        }
    }

    // shaders
    const JsonValue& shaders = root.GetField("shaders");
    if (shaders.IsArray()) {
        for (const auto& v : shaders.arrayValue) {
            if (!v.IsObject()) continue;
            HFShaderDesc d;
            d.name = v.GetField("name").GetString();
            d.path = v.GetField("path").GetString();
            d.type = v.GetField("type").GetString("glsl");
            d.stage = v.GetField("stage").GetString("fragment");
            m.shaders.push_back(std::move(d));
        }
    }

    // meshes
    const JsonValue& meshes = root.GetField("meshes");
    if (meshes.IsArray()) {
        for (const auto& v : meshes.arrayValue) {
            if (!v.IsObject()) continue;
            HFMeshDesc d;
            d.name = v.GetField("name").GetString();
            d.path = v.GetField("path").GetString();
            d.format = v.GetField("format").GetString("json");
            m.meshes.push_back(std::move(d));
        }
    }

    // parameters
    const JsonValue& params = root.GetField("parameters");
    if (params.IsArray()) {
        for (const auto& v : params.arrayValue) {
            if (!v.IsObject()) continue;
            HFParameterDesc d;
            d.name = v.GetField("name").GetString();
            d.type = v.GetField("type").GetString("float");
            if (v.HasField("min")) {
                d.hasMin = true;
                d.minValue = v.GetField("min").GetNumber();
            }
            if (v.HasField("max")) {
                d.hasMax = true;
                d.maxValue = v.GetField("max").GetNumber();
            }
            if (v.HasField("default")) {
                d.hasDefault = true;
                d.defaultValue = v.GetField("default");
            }
            if (v.HasField("options")) {
                const JsonValue& opts = v.GetField("options");
                if (opts.IsArray()) {
                    for (const auto& o : opts.arrayValue) {
                        if (o.IsString()) d.options.push_back(o.stringValue);
                    }
                }
            }
            d.group = v.GetField("group").GetString();
            d.description = v.GetField("description").GetString();
            m.parameters.push_back(std::move(d));
        }
    }

    // passes
    const JsonValue& passes = root.GetField("passes");
    if (passes.IsArray()) {
        for (const auto& v : passes.arrayValue) {
            if (!v.IsObject()) continue;
            HFPassDesc d;
            d.name = v.GetField("name").GetString();
            d.shader = v.GetField("shader").GetString();
            d.blend = v.GetField("blend").GetString("normal");
            d.order = (int)v.GetField("order").GetNumber(0);
            const JsonValue& texArr = v.GetField("textures");
            if (texArr.IsArray()) {
                for (const auto& t : texArr.arrayValue) {
                    if (t.IsString()) d.textures.push_back(t.stringValue);
                }
            }
            const JsonValue& maskArr = v.GetField("masks");
            if (maskArr.IsArray()) {
                for (const auto& mm : maskArr.arrayValue) {
                    if (mm.IsString()) d.masks.push_back(mm.stringValue);
                }
            }
            m.passes.push_back(std::move(d));
        }
        // Sort by order
        std::sort(m.passes.begin(), m.passes.end(), [](const HFPassDesc& a, const HFPassDesc& b) {
            return a.order < b.order;
        });
    }

    return m;
}

std::string ManifestParser::Validate(const HFManifest& m) {
    if (m.format.empty()) return "Missing required field: format";
    if (m.format != "HuanFaceBundle") return "Invalid format, must be HuanFaceBundle, got: " + m.format;
    if (m.version.empty()) return "Missing required field: version";
    if (m.type.empty()) return "Missing required field: type";
    if (m.name.empty()) return "Missing required field: name";

    // Validate type
    static const std::vector<std::string> validTypes = {"makeup", "beauty", "filter", "effect", "hair", "face", "combined"};
    bool typeOk = false;
    for (const auto& t : validTypes) if (t == m.type) { typeOk = true; break; }
    if (!typeOk) {
        // Allow unknown but warn? For validation we allow but could be strict.
        // We'll allow unknown types with warning, not error.
    }

    // Validate version format simple: should contain dot or be numeric
    // Not strict

    // Validate paths for traversal
    auto checkPath = [](const std::string& p) -> bool {
        return !ManifestParser::IsPathTraversal(p);
    };

    for (const auto& tex : m.textures) {
        if (tex.name.empty()) return "Texture missing name";
        if (tex.path.empty()) return "Texture " + tex.name + " missing path";
        if (!checkPath(tex.path)) return "Texture path traversal detected: " + tex.path;
    }
    for (const auto& mask : m.masks) {
        if (mask.name.empty()) return "Mask missing name";
        if (mask.path.empty()) return "Mask " + mask.name + " missing path";
        if (!checkPath(mask.path)) return "Mask path traversal detected: " + mask.path;
    }
    for (const auto& sh : m.shaders) {
        if (sh.name.empty()) return "Shader missing name";
        if (sh.path.empty()) return "Shader " + sh.name + " missing path";
        if (!checkPath(sh.path)) return "Shader path traversal detected: " + sh.path;
    }
    for (const auto& mesh : m.meshes) {
        if (mesh.name.empty()) return "Mesh missing name";
        if (mesh.path.empty()) return "Mesh " + mesh.name + " missing path";
        if (!checkPath(mesh.path)) return "Mesh path traversal detected: " + mesh.path;
    }
    for (const auto& param : m.parameters) {
        if (param.name.empty()) return "Parameter missing name";
        if (param.type.empty()) return "Parameter " + param.name + " missing type";
        static const std::vector<std::string> validParamTypes = {"float", "int", "bool", "color", "vec2", "vec3", "vec4", "texture", "enum"};
        bool ok = false;
        for (const auto& vt : validParamTypes) if (vt == param.type) { ok = true; break; }
        if (!ok) return "Parameter " + param.name + " has invalid type: " + param.type;
        if (param.hasMin && param.hasMax && param.minValue > param.maxValue) {
            return "Parameter " + param.name + " min > max";
        }
    }
    for (const auto& pass : m.passes) {
        if (pass.name.empty()) return "Pass missing name";
        if (pass.shader.empty()) return "Pass " + pass.name + " missing shader";
        if (!checkPath(pass.shader)) return "Pass shader path traversal: " + pass.shader;
        for (const auto& tp : pass.textures) {
            if (!checkPath(tp)) return "Pass texture path traversal: " + tp;
        }
        for (const auto& mp : pass.masks) {
            // Masks can be logical names without slash, so only check if contains traversal or absolute
            if (mp.find("..") != std::string::npos || (!mp.empty() && (mp[0] == '/' || mp[0] == '\\'))) {
                return "Pass mask path traversal: " + mp;
            }
        }
    }

    return ""; // OK
}

} // namespace huanface
