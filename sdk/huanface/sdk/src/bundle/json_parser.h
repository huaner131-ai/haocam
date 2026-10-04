/**
 * HuanFace Minimal JSON Parser — header-only, MIT, clean-room
 * For parsing manifest.json in .hfbundle
 * Supports: null, bool, number, string, array, object
 * No external dependency
 * 
 * Usage:
 *   JsonValue root = JsonParser::Parse(jsonString);
 *   if (root.type == JsonType::Object) { ... }
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <stdexcept>
#include <sstream>

namespace huanface {

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::map<std::string, JsonValue> objectValue;

    JsonValue() : type(JsonType::Null) {}
    JsonValue(bool b) : type(JsonType::Bool), boolValue(b) {}
    JsonValue(double n) : type(JsonType::Number), numberValue(n) {}
    JsonValue(const std::string& s) : type(JsonType::String), stringValue(s) {}
    JsonValue(const char* s) : type(JsonType::String), stringValue(s) {}

    bool IsNull() const { return type == JsonType::Null; }
    bool IsBool() const { return type == JsonType::Bool; }
    bool IsNumber() const { return type == JsonType::Number; }
    bool IsString() const { return type == JsonType::String; }
    bool IsArray() const { return type == JsonType::Array; }
    bool IsObject() const { return type == JsonType::Object; }

    // Helpers to get object field
    bool HasField(const std::string& key) const {
        if (type != JsonType::Object) return false;
        return objectValue.find(key) != objectValue.end();
    }

    const JsonValue& GetField(const std::string& key) const {
        static JsonValue nullVal;
        if (type != JsonType::Object) return nullVal;
        auto it = objectValue.find(key);
        if (it == objectValue.end()) return nullVal;
        return it->second;
    }

    std::string GetString(const std::string& def = "") const {
        if (type == JsonType::String) return stringValue;
        return def;
    }

    double GetNumber(double def = 0.0) const {
        if (type == JsonType::Number) return numberValue;
        return def;
    }

    bool GetBool(bool def = false) const {
        if (type == JsonType::Bool) return boolValue;
        return def;
    }

    // For array iteration
    size_t ArraySize() const {
        if (type == JsonType::Array) return arrayValue.size();
        return 0;
    }
};

class JsonParser {
public:
    static JsonValue Parse(const std::string& json) {
        JsonParser p(json);
        JsonValue v = p.ParseValue();
        p.SkipWhitespace();
        if (p.pos != p.json.size()) {
            // Allow trailing whitespace only
            p.SkipWhitespace();
            if (p.pos != p.json.size()) {
                throw std::runtime_error("Unexpected trailing characters after JSON");
            }
        }
        return v;
    }

private:
    std::string json;
    size_t pos = 0;

    JsonParser(const std::string& j) : json(j), pos(0) {}

    void SkipWhitespace() {
        while (pos < json.size() && std::isspace((unsigned char)json[pos])) pos++;
    }

    char Peek() {
        if (pos >= json.size()) return '\0';
        return json[pos];
    }

    char Next() {
        if (pos >= json.size()) return '\0';
        return json[pos++];
    }

    JsonValue ParseValue() {
        SkipWhitespace();
        char c = Peek();
        if (c == '\0') throw std::runtime_error("Unexpected end of JSON");
        if (c == 'n') return ParseNull();
        if (c == 't' || c == 'f') return ParseBool();
        if (c == '"') return ParseString();
        if (c == '[') return ParseArray();
        if (c == '{') return ParseObject();
        if (c == '-' || std::isdigit((unsigned char)c)) return ParseNumber();
        throw std::runtime_error(std::string("Unexpected character in JSON: ") + c);
    }

    JsonValue ParseNull() {
        if (json.compare(pos, 4, "null") == 0) {
            pos += 4;
            return JsonValue();
        }
        throw std::runtime_error("Invalid null");
    }

    JsonValue ParseBool() {
        if (json.compare(pos, 4, "true") == 0) {
            pos += 4;
            return JsonValue(true);
        }
        if (json.compare(pos, 5, "false") == 0) {
            pos += 5;
            return JsonValue(false);
        }
        throw std::runtime_error("Invalid bool");
    }

    JsonValue ParseNumber() {
        size_t start = pos;
        if (Peek() == '-') Next();
        while (pos < json.size() && std::isdigit((unsigned char)json[pos])) pos++;
        if (pos < json.size() && json[pos] == '.') {
            pos++;
            while (pos < json.size() && std::isdigit((unsigned char)json[pos])) pos++;
        }
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            pos++;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) pos++;
            while (pos < json.size() && std::isdigit((unsigned char)json[pos])) pos++;
        }
        std::string numStr = json.substr(start, pos - start);
        try {
            double val = std::stod(numStr);
            return JsonValue(val);
        } catch (...) {
            throw std::runtime_error("Invalid number: " + numStr);
        }
    }

    JsonValue ParseString() {
        // Assume starting quote
        if (Next() != '"') throw std::runtime_error("Expected opening quote for string");
        std::string result;
        while (pos < json.size()) {
            char c = Next();
            if (c == '"') {
                return JsonValue(result);
            }
            if (c == '\\') {
                if (pos >= json.size()) throw std::runtime_error("Invalid escape at end of string");
                char esc = Next();
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        // Unicode \uXXXX — parse 4 hex digits, convert to UTF-8 (basic)
                        if (pos + 4 > json.size()) throw std::runtime_error("Invalid unicode escape");
                        std::string hex = json.substr(pos, 4);
                        pos += 4;
                        try {
                            int code = std::stoi(hex, nullptr, 16);
                            // Basic UTF-8 encoding for BMP
                            if (code <= 0x7F) {
                                result += (char)code;
                            } else if (code <= 0x7FF) {
                                result += (char)(0xC0 | (code >> 6));
                                result += (char)(0x80 | (code & 0x3F));
                            } else {
                                result += (char)(0xE0 | (code >> 12));
                                result += (char)(0x80 | ((code >> 6) & 0x3F));
                                result += (char)(0x80 | (code & 0x3F));
                            }
                        } catch (...) {
                            throw std::runtime_error("Invalid unicode escape: \\u" + hex);
                        }
                        break;
                    }
                    default:
                        throw std::runtime_error(std::string("Invalid escape: \\") + esc);
                }
            } else {
                result += c;
            }
        }
        throw std::runtime_error("Unterminated string");
    }

    JsonValue ParseArray() {
        if (Next() != '[') throw std::runtime_error("Expected [ for array");
        JsonValue arr;
        arr.type = JsonType::Array;
        SkipWhitespace();
        if (Peek() == ']') {
            Next();
            return arr;
        }
        while (true) {
            JsonValue val = ParseValue();
            arr.arrayValue.push_back(std::move(val));
            SkipWhitespace();
            char c = Next();
            if (c == ']') break;
            if (c != ',') throw std::runtime_error("Expected , or ] in array");
            SkipWhitespace();
        }
        return arr;
    }

    JsonValue ParseObject() {
        if (Next() != '{') throw std::runtime_error("Expected { for object");
        JsonValue obj;
        obj.type = JsonType::Object;
        SkipWhitespace();
        if (Peek() == '}') {
            Next();
            return obj;
        }
        while (true) {
            SkipWhitespace();
            if (Peek() != '"') throw std::runtime_error("Expected string key in object");
            JsonValue key = ParseString();
            SkipWhitespace();
            if (Next() != ':') throw std::runtime_error("Expected : after key in object");
            JsonValue val = ParseValue();
            obj.objectValue[key.stringValue] = std::move(val);
            SkipWhitespace();
            char c = Next();
            if (c == '}') break;
            if (c != ',') throw std::runtime_error("Expected , or } in object");
            SkipWhitespace();
        }
        return obj;
    }
};

} // namespace huanface
