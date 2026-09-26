#include "core/config/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace haocam::core {

namespace {

struct Parser {
    std::string_view text;
    size_t pos = 0;
    std::string error;
    int depth = 0;

    static constexpr int kMaxDepth = 64;

    bool fail(const std::string& message) {
        if (error.empty()) {
            size_t line = 1;
            size_t lastLineStart = 0;
            for (size_t i = 0; i < pos && i < text.size(); ++i) {
                if (text[i] == '\n') {
                    ++line;
                    lastLineStart = i + 1;
                }
            }
            char buf[64];
            std::snprintf(buf, sizeof(buf), " at line %zu, column %zu", line, pos - lastLineStart + 1);
            error = message + buf;
        }
        return false;
    }

    void skipWhitespace() {
        while (pos < text.size()) {
            const char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos;
            } else {
                break;
            }
        }
    }

    bool eof() const { return pos >= text.size(); }
    char peek() const { return pos < text.size() ? text[pos] : '\0'; }

    bool consume(char c) {
        if (peek() == c) {
            ++pos;
            return true;
        }
        return false;
    }

    bool expect(char c) {
        if (consume(c)) return true;
        char buf[48];
        std::snprintf(buf, sizeof(buf), "Expected '%c'", c);
        return fail(buf);
    }

    static void appendUtf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool parseHex4(uint32_t& out) {
        if (pos + 4 > text.size()) return fail("Truncated \\u escape");
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text[pos + static_cast<size_t>(i)];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<uint32_t>(c - 'A' + 10);
            else return fail("Invalid hex digit in \\u escape");
        }
        pos += 4;
        out = value;
        return true;
    }

    bool parseString(std::string& out) {
        if (!expect('"')) return false;
        out.clear();
        while (true) {
            if (eof()) return fail("Unterminated string");
            const char c = text[pos++];
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (eof()) return fail("Unterminated escape sequence");
            const char esc = text[pos++];
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    uint32_t cp = 0;
                    if (!parseHex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) { // high surrogate
                        if (pos + 1 < text.size() && text[pos] == '\\' && text[pos + 1] == 'u') {
                            pos += 2;
                            uint32_t low = 0;
                            if (!parseHex4(low)) return false;
                            if (low >= 0xDC00 && low <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                            } else {
                                appendUtf8(out, 0xFFFD);
                                cp = 0xFFFD;
                            }
                        } else {
                            cp = 0xFFFD;
                        }
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        cp = 0xFFFD; // orphan low surrogate
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default:
                    return fail("Invalid escape sequence");
            }
        }
    }

    bool parseNumber(double& out) {
        const size_t start = pos;
        if (consume('-')) {}
        while (!eof() && text[pos] >= '0' && text[pos] <= '9') ++pos;
        if (consume('.')) {
            while (!eof() && text[pos] >= '0' && text[pos] <= '9') ++pos;
        }
        if (!eof() && (text[pos] == 'e' || text[pos] == 'E')) {
            ++pos;
            consume('+') || consume('-');
            while (!eof() && text[pos] >= '0' && text[pos] <= '9') ++pos;
        }
        if (pos == start) return fail("Invalid number");
        const std::string token{text.substr(start, pos - start)};
        char* end = nullptr;
        out = std::strtod(token.c_str(), &end);
        if (end != token.c_str() + token.size()) return fail("Invalid number");
        return true;
    }

    bool parseValue(JsonValue& out) {
        if (++depth > kMaxDepth) {
            --depth;
            return fail("Maximum nesting depth exceeded");
        }
        skipWhitespace();
        const char c = peek();
        bool ok = false;
        if (c == '{') ok = parseObject(out);
        else if (c == '[') ok = parseArray(out);
        else if (c == '"') {
            std::string s;
            if ((ok = parseString(s))) out = JsonValue(std::move(s));
        } else if (c == 't') {
            if (match("true")) { out = JsonValue(true); ok = true; }
            else ok = fail("Invalid literal");
        } else if (c == 'f') {
            if (match("false")) { out = JsonValue(false); ok = true; }
            else ok = fail("Invalid literal");
        } else if (c == 'n') {
            if (match("null")) { out = JsonValue(); ok = true; }
            else ok = fail("Invalid literal");
        } else {
            double n = 0;
            if ((ok = parseNumber(n))) out = JsonValue(n);
        }
        --depth;
        return ok;
    }

    bool match(std::string_view keyword) {
        if (text.size() - pos >= keyword.size() && text.compare(pos, keyword.size(), keyword) == 0) {
            pos += keyword.size();
            return true;
        }
        return false;
    }

    bool parseObject(JsonValue& out) {
        if (!expect('{')) return false;
        out = JsonValue::makeObject();
        skipWhitespace();
        if (consume('}')) return true;
        while (true) {
            skipWhitespace();
            std::string key;
            if (!parseString(key)) return false;
            skipWhitespace();
            if (!expect(':')) return false;
            JsonValue value;
            if (!parseValue(value)) return false;
            out.set(std::move(key), std::move(value));
            skipWhitespace();
            if (consume(',')) continue;
            if (consume('}')) return true;
            return fail("Expected ',' or '}' in object");
        }
    }

    bool parseArray(JsonValue& out) {
        if (!expect('[')) return false;
        out = JsonValue::makeArray();
        skipWhitespace();
        if (consume(']')) return true;
        while (true) {
            JsonValue value;
            if (!parseValue(value)) return false;
            out.append(std::move(value));
            skipWhitespace();
            if (consume(',')) continue;
            if (consume(']')) return true;
            return fail("Expected ',' or ']' in array");
        }
    }
};

void serializeString(std::string& out, const std::string& value) {
    out.push_back('"');
    for (const char raw : value) {
        const unsigned char c = static_cast<unsigned char>(raw);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(raw);
                }
        }
    }
    out.push_back('"');
}

void serializeNumber(std::string& out, double value) {
    if (std::isfinite(value) && value == static_cast<double>(static_cast<long long>(value)) &&
        std::abs(value) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(value));
        out += buf;
    } else {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.17g", value);
        out += buf;
    }
}

} // namespace

JsonValue::JsonValue(const JsonValue& other)
    : m_type(other.m_type),
      m_bool(other.m_bool),
      m_number(other.m_number),
      m_string(other.m_string) {
    if (other.m_array) m_array = std::make_unique<Array>(*other.m_array);
    if (other.m_object) m_object = std::make_unique<Object>(*other.m_object);
}

JsonValue::JsonValue(JsonValue&& other) noexcept
    : m_type(other.m_type),
      m_bool(other.m_bool),
      m_number(other.m_number),
      m_string(std::move(other.m_string)),
      m_array(std::move(other.m_array)),
      m_object(std::move(other.m_object)) {
    other.m_type = Type::Null;
}

JsonValue& JsonValue::operator=(const JsonValue& other) {
    if (this != &other) {
        JsonValue copy(other);
        *this = std::move(copy);
    }
    return *this;
}

JsonValue& JsonValue::operator=(JsonValue&& other) noexcept {
    if (this != &other) {
        m_type = other.m_type;
        m_bool = other.m_bool;
        m_number = other.m_number;
        m_string = std::move(other.m_string);
        m_array = std::move(other.m_array);
        m_object = std::move(other.m_object);
        other.m_type = Type::Null;
    }
    return *this;
}

JsonValue JsonValue::makeObject() {
    JsonValue v;
    v.m_type = Type::Object;
    v.m_object = std::make_unique<Object>();
    return v;
}

JsonValue JsonValue::makeArray() {
    JsonValue v;
    v.m_type = Type::Array;
    v.m_array = std::make_unique<Array>();
    return v;
}

void JsonValue::clear() {
    m_type = Type::Null;
    m_bool = false;
    m_number = 0.0;
    m_string.clear();
    m_array.reset();
    m_object.reset();
}

JsonValue JsonValue::parse(std::string_view text, std::string* error) {
    Parser parser;
    parser.text = text;
    JsonValue value;
    if (!parser.parseValue(value)) {
        if (error) *error = parser.error;
        return JsonValue();
    }
    parser.skipWhitespace();
    if (!parser.eof()) {
        if (error) *error = "Trailing content after JSON value";
        return JsonValue();
    }
    if (error) error->clear();
    return value;
}

bool JsonValue::asBool(bool fallback) const {
    return m_type == Type::Bool ? m_bool : fallback;
}

double JsonValue::asNumber(double fallback) const {
    return m_type == Type::Number ? m_number : fallback;
}

int JsonValue::asInt(int fallback) const {
    return m_type == Type::Number ? static_cast<int>(m_number) : fallback;
}

std::string JsonValue::asString(std::string fallback) const {
    return m_type == Type::String ? m_string : std::move(fallback);
}

bool JsonValue::contains(std::string_view key) const {
    if (m_type != Type::Object || !m_object) return false;
    for (const auto& [k, v] : *m_object) {
        if (k == key) return true;
    }
    return false;
}

const JsonValue* JsonValue::find(std::string_view key) const {
    if (m_type != Type::Object || !m_object) return nullptr;
    for (const auto& [k, v] : *m_object) {
        if (k == key) return &v;
    }
    return nullptr;
}

JsonValue& JsonValue::set(std::string key, JsonValue value) {
    if (m_type != Type::Object) {
        clear();
        m_type = Type::Object;
        m_object = std::make_unique<Object>();
    }
    for (auto& [k, v] : *m_object) {
        if (k == key) {
            v = std::move(value);
            return v;
        }
    }
    m_object->emplace_back(std::move(key), std::move(value));
    return m_object->back().second;
}

JsonValue& JsonValue::operator[](std::string_view key) {
    if (m_type != Type::Object) {
        clear();
        m_type = Type::Object;
        m_object = std::make_unique<Object>();
    }
    for (auto& [k, v] : *m_object) {
        if (k == key) return v;
    }
    m_object->emplace_back(std::string(key), JsonValue());
    return m_object->back().second;
}

const JsonValue& JsonValue::at(std::string_view key) const {
    static const JsonValue kNull;
    if (const JsonValue* v = find(key)) return *v;
    return kNull;
}

size_t JsonValue::size() const {
    if (m_type == Type::Array && m_array) return m_array->size();
    if (m_type == Type::Object && m_object) return m_object->size();
    return 0;
}

const JsonValue& JsonValue::at(size_t index) const {
    static const JsonValue kNull;
    if (m_type == Type::Array && m_array && index < m_array->size()) {
        return (*m_array)[index];
    }
    return kNull;
}

void JsonValue::append(JsonValue value) {
    if (m_type != Type::Array) {
        clear();
        m_type = Type::Array;
        m_array = std::make_unique<Array>();
    }
    m_array->push_back(std::move(value));
}

void JsonValue::serializeTo(std::string& out, int indent, int depth) const {
    const auto newline = [&](int d) {
        if (indent > 0) {
            out.push_back('\n');
            out.append(static_cast<size_t>(indent * d), ' ');
        }
    };

    switch (m_type) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += m_bool ? "true" : "false"; break;
        case Type::Number: serializeNumber(out, m_number); break;
        case Type::String: serializeString(out, m_string); break;
        case Type::Array: {
            if (!m_array || m_array->empty()) {
                out += "[]";
                break;
            }
            out.push_back('[');
            for (size_t i = 0; i < m_array->size(); ++i) {
                if (i) out.push_back(',');
                newline(depth + 1);
                (*m_array)[i].serializeTo(out, indent, depth + 1);
            }
            newline(depth);
            out.push_back(']');
            break;
        }
        case Type::Object: {
            if (!m_object || m_object->empty()) {
                out += "{}";
                break;
            }
            out.push_back('{');
            for (size_t i = 0; i < m_object->size(); ++i) {
                if (i) out.push_back(',');
                newline(depth + 1);
                serializeString(out, (*m_object)[i].first);
                out.push_back(indent > 0 ? ':' : ':');
                if (indent > 0) out.push_back(' ');
                (*m_object)[i].second.serializeTo(out, indent, depth + 1);
            }
            newline(depth);
            out.push_back('}');
            break;
        }
    }
}

std::string JsonValue::serialize(int indent) const {
    std::string out;
    out.reserve(256);
    serializeTo(out, indent, 0);
    return out;
}

} // namespace haocam::core
