#pragma once

// Minimal, dependency-free JSON for HaoCam configuration and presets.
//
// Supports the full JSON grammar (objects, arrays, strings with \u escapes
// and surrogate pairs, numbers, booleans, null). Object key order is
// preserved so round-tripped settings files stay human-friendly.

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace haocam::core {

class JsonValue final {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    using Array = std::vector<JsonValue>;
    using Object = std::vector<std::pair<std::string, JsonValue>>;

    JsonValue() : m_type(Type::Null) {}
    explicit JsonValue(bool value) : m_type(Type::Bool), m_bool(value) {}
    explicit JsonValue(double value) : m_type(Type::Number), m_number(value) {}
    explicit JsonValue(int value) : m_type(Type::Number), m_number(value) {}
    explicit JsonValue(std::string value) : m_type(Type::String), m_string(std::move(value)) {}
    explicit JsonValue(const char* value) : m_type(Type::String), m_string(value) {}

    JsonValue(const JsonValue& other);
    JsonValue(JsonValue&& other) noexcept;
    JsonValue& operator=(const JsonValue& other);
    JsonValue& operator=(JsonValue&& other) noexcept;
    ~JsonValue() = default;

    static JsonValue makeObject();
    static JsonValue makeArray();

    // Parses `text`. On failure returns a Null value and (optionally) writes a
    // human readable error including line/column information.
    static JsonValue parse(std::string_view text, std::string* error = nullptr);

    Type type() const { return m_type; }
    bool isNull() const { return m_type == Type::Null; }
    bool isBool() const { return m_type == Type::Bool; }
    bool isNumber() const { return m_type == Type::Number; }
    bool isString() const { return m_type == Type::String; }
    bool isArray() const { return m_type == Type::Array; }
    bool isObject() const { return m_type == Type::Object; }

    // Typed accessors with fallback defaults (never throw).
    bool asBool(bool fallback = false) const;
    double asNumber(double fallback = 0.0) const;
    int asInt(int fallback = 0) const;
    std::string asString(std::string fallback = {}) const;

    // Object/array access.
    bool contains(std::string_view key) const;
    const JsonValue* find(std::string_view key) const;
    JsonValue& set(std::string key, JsonValue value);
    JsonValue& operator[](std::string_view key); // creates missing keys (must be object)
    const JsonValue& at(std::string_view key) const; // returns static null if missing

    size_t size() const;
    const JsonValue& at(size_t index) const;
    void append(JsonValue value);

    // Serializes the value. `indent` = 0 produces compact output; a positive
    // indent produces pretty-printed JSON with the given spaces per level.
    std::string serialize(int indent = 0) const;

    void clear();

private:
    void serializeTo(std::string& out, int indent, int depth) const;

    Type m_type = Type::Null;
    bool m_bool = false;
    double m_number = 0.0;
    std::string m_string;
    std::unique_ptr<Array> m_array;
    std::unique_ptr<Object> m_object;
};

} // namespace haocam::core
