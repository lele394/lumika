#include "e3d/core/json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace e3d::json {

Value Value::boolean(bool value) {
    Value v;
    v.type_ = Type::Bool;
    v.bool_value_ = value;
    return v;
}

Value Value::number(double value) {
    Value v;
    v.type_ = Type::Number;
    v.number_value_ = value;
    return v;
}

Value Value::string(std::string_view value) {
    Value v;
    v.type_ = Type::String;
    v.string_value_.assign(value);
    return v;
}

Value Value::array() {
    Value v;
    v.type_ = Type::Array;
    return v;
}

Value Value::object() {
    Value v;
    v.type_ = Type::Object;
    return v;
}

const Value* Value::find(std::string_view key) const noexcept {
    if (type_ != Type::Object) return nullptr;
    for (const auto& entry : object_) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

Value* Value::find(std::string_view key) noexcept {
    if (type_ != Type::Object) return nullptr;
    for (auto& entry : object_) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

void Value::set(std::string_view key, Value value) {
    if (type_ != Type::Object) {
        *this = object();
    }
    for (auto& entry : object_) {
        if (entry.first == key) {
            entry.second = std::move(value);
            return;
        }
    }
    object_.emplace_back(std::string(key), std::move(value));
}

void Value::push(Value value) {
    if (type_ != Type::Array) {
        *this = array();
    }
    array_.push_back(std::move(value));
}

namespace {

int hex_digit(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void append_utf8(std::string& out, u32 codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

class Parser final {
public:
    explicit Parser(std::string_view text) : src_(text) {}

    bool run(Value& out, ParseError& error) {
        if (src_.size() >= 3 && static_cast<u8>(src_[0]) == 0xEF && static_cast<u8>(src_[1]) == 0xBB &&
            static_cast<u8>(src_[2]) == 0xBF) {
            pos_ = 3;
        }

        skip_ws();
        if (!parse_value(out)) {
            error.offset = error_pos_;
            error.message = error_;
            return false;
        }
        skip_ws();
        if (pos_ != src_.size()) {
            fail("trailing characters");
            error.offset = error_pos_;
            error.message = error_;
            return false;
        }
        return true;
    }

private:
    bool fail(const char* message) {
        if (error_.empty()) {
            error_ = message;
            error_pos_ = pos_;
        }
        return false;
    }

    bool at_end() const noexcept { return pos_ >= src_.size(); }
    char peek() const noexcept { return at_end() ? '\0' : src_[pos_]; }

    void skip_ws() noexcept {
        while (!at_end()) {
            const char c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool expect(char c) {
        if (peek() != c) return fail("unexpected character");
        ++pos_;
        return true;
    }

    bool parse_value(Value& out) {
        if (depth_ >= 100) return fail("nesting too deep");
        ++depth_;

        bool ok = false;
        switch (peek()) {
            case '{': ok = parse_object(out); break;
            case '[': ok = parse_array(out); break;
            case '"': {
                std::string text;
                ok = parse_string(text);
                if (ok) out = Value::string(text);
                break;
            }
            case 't':
                if (src_.substr(pos_, 4) == "true") {
                    pos_ += 4;
                    out = Value::boolean(true);
                    ok = true;
                } else {
                    ok = fail("invalid literal");
                }
                break;
            case 'f':
                if (src_.substr(pos_, 5) == "false") {
                    pos_ += 5;
                    out = Value::boolean(false);
                    ok = true;
                } else {
                    ok = fail("invalid literal");
                }
                break;
            case 'n':
                if (src_.substr(pos_, 4) == "null") {
                    pos_ += 4;
                    out = Value();
                    ok = true;
                } else {
                    ok = fail("invalid literal");
                }
                break;
            default:
                ok = parse_number(out);
                break;
        }

        --depth_;
        return ok;
    }

    bool parse_object(Value& out) {
        if (!expect('{')) return false;
        out = Value::object();
        skip_ws();
        if (peek() == '}') {
            ++pos_;
            return true;
        }

        for (;;) {
            skip_ws();
            if (peek() != '"') return fail("expected object key");
            std::string key;
            if (!parse_string(key)) return false;

            skip_ws();
            if (!expect(':')) return false;
            skip_ws();

            Value value;
            if (!parse_value(value)) return false;
            out.set(key, std::move(value));

            skip_ws();
            const char c = peek();
            if (c == ',') {
                ++pos_;
                continue;
            }
            if (c == '}') {
                ++pos_;
                return true;
            }
            return fail("expected ',' or '}'");
        }
    }

    bool parse_array(Value& out) {
        if (!expect('[')) return false;
        out = Value::array();
        skip_ws();
        if (peek() == ']') {
            ++pos_;
            return true;
        }

        for (;;) {
            skip_ws();
            Value value;
            if (!parse_value(value)) return false;
            out.push(std::move(value));

            skip_ws();
            const char c = peek();
            if (c == ',') {
                ++pos_;
                continue;
            }
            if (c == ']') {
                ++pos_;
                return true;
            }
            return fail("expected ',' or ']'");
        }
    }

    bool read_hex4(u32& result) {
        if (pos_ + 4 > src_.size()) return fail("truncated unicode escape");
        u32 value = 0;
        for (int i = 0; i < 4; ++i) {
            const int digit = hex_digit(src_[pos_++]);
            if (digit < 0) return fail("invalid unicode escape");
            value = (value << 4) | static_cast<u32>(digit);
        }
        result = value;
        return true;
    }

    bool parse_string(std::string& out) {
        if (!expect('"')) return false;
        out.clear();

        while (!at_end()) {
            const char c = src_[pos_++];
            if (c == '"') return true;
            if (static_cast<u8>(c) < 0x20) return fail("control character in string");

            if (c != '\\') {
                out.push_back(c);
                continue;
            }

            if (at_end()) return fail("truncated escape");
            const char esc = src_[pos_++];
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
                    u32 codepoint = 0;
                    if (!read_hex4(codepoint)) return false;
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                        if (pos_ + 2 > src_.size() || src_[pos_] != '\\' || src_[pos_ + 1] != 'u') {
                            return fail("missing low surrogate");
                        }
                        pos_ += 2;
                        u32 low = 0;
                        if (!read_hex4(low)) return false;
                        if (low < 0xDC00 || low > 0xDFFF) return fail("invalid low surrogate");
                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                    }
                    append_utf8(out, codepoint);
                    break;
                }
                default: return fail("invalid escape");
            }
        }

        return fail("unterminated string");
    }

    bool parse_number(Value& out) {
        const std::size_t start = pos_;

        if (peek() == '-') ++pos_;
        if (peek() < '0' || peek() > '9') return fail("invalid number");

        if (peek() == '0') {
            ++pos_;
        } else {
            while (peek() >= '0' && peek() <= '9') ++pos_;
        }

        if (peek() == '.') {
            ++pos_;
            if (peek() < '0' || peek() > '9') return fail("expected digit after '.'");
            while (peek() >= '0' && peek() <= '9') ++pos_;
        }

        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') ++pos_;
            if (peek() < '0' || peek() > '9') return fail("expected exponent digits");
            while (peek() >= '0' && peek() <= '9') ++pos_;
        }

        const std::size_t length = pos_ - start;
        if (length >= 64) return fail("number too long");

        char buffer[64];
        std::memcpy(buffer, src_.data() + start, length);
        buffer[length] = '\0';

        char* end = nullptr;
        const double value = std::strtod(buffer, &end);
        if (end != buffer + length) return fail("malformed number");

        out = Value::number(value);
        return true;
    }

    std::string_view src_;
    std::size_t pos_ = 0;
    int depth_ = 0;
    std::string error_;
    std::size_t error_pos_ = 0;
};

void dump_string(const std::string& text, std::string& out) {
    out.push_back('"');
    for (unsigned char c : text) {
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
                    char escape[8];
                    std::snprintf(escape, sizeof(escape), "\\u%04x", c);
                    out += escape;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
}

void dump_number(double value, std::string& out) {
    if (!std::isfinite(value)) {
        out += "null";
        return;
    }

    char buffer[40];
    if (value == std::floor(value) && std::fabs(value) < 1e15) {
        std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.9g", value);
    }
    out += buffer;
}

void dump_value(const Value& value, std::string& out, bool pretty, int indent_width, int level) {
    const auto newline = [&](int depth) {
        if (!pretty) return;
        out.push_back('\n');
        out.append(static_cast<std::size_t>(depth) * static_cast<std::size_t>(indent_width), ' ');
    };

    switch (value.type()) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += value.as_bool() ? "true" : "false"; break;
        case Type::Number: dump_number(value.as_number(), out); break;
        case Type::String: dump_string(value.as_string(), out); break;
        case Type::Array: {
            if (value.size() == 0) {
                out += "[]";
                break;
            }
            out.push_back('[');
            for (std::size_t i = 0; i < value.size(); ++i) {
                if (i != 0) out.push_back(',');
                newline(level + 1);
                dump_value(value[i], out, pretty, indent_width, level + 1);
            }
            newline(level);
            out.push_back(']');
            break;
        }
        case Type::Object: {
            if (value.size() == 0) {
                out += "{}";
                break;
            }
            out.push_back('{');
            for (std::size_t i = 0; i < value.size(); ++i) {
                if (i != 0) out.push_back(',');
                newline(level + 1);
                dump_string(value.key_at(i), out);
                out += pretty ? ": " : ":";
                dump_value(value.value_at(i), out, pretty, indent_width, level + 1);
            }
            newline(level);
            out.push_back('}');
            break;
        }
    }
}

} // namespace

bool parse(std::string_view text, Value& out, ParseError& error) {
    error = {};
    Parser parser(text);
    return parser.run(out, error);
}

std::string dump(const Value& value, bool pretty, int indent) {
    std::string out;
    out.reserve(1024);
    dump_value(value, out, pretty, indent, 0);
    if (pretty) out.push_back('\n');
    return out;
}

} // namespace e3d::json
