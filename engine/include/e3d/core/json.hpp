#pragma once

#include "e3d/core/types.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace e3d::json {

enum class Type : u8 { Null, Bool, Number, String, Array, Object };

/// Small JSON DOM. Preserves object key order, which keeps serialized scenes
/// deterministic and lets unknown keys round-trip untouched.
class Value final {
public:
    Value() = default;

    static Value boolean(bool value);
    static Value number(double value);
    static Value string(std::string_view value);
    static Value array();
    static Value object();

    [[nodiscard]] Type type() const noexcept { return type_; }
    [[nodiscard]] bool is_null() const noexcept { return type_ == Type::Null; }
    [[nodiscard]] bool is_bool() const noexcept { return type_ == Type::Bool; }
    [[nodiscard]] bool is_number() const noexcept { return type_ == Type::Number; }
    [[nodiscard]] bool is_string() const noexcept { return type_ == Type::String; }
    [[nodiscard]] bool is_array() const noexcept { return type_ == Type::Array; }
    [[nodiscard]] bool is_object() const noexcept { return type_ == Type::Object; }

    [[nodiscard]] bool as_bool(bool fallback = false) const noexcept {
        return type_ == Type::Bool ? bool_value_ : fallback;
    }
    [[nodiscard]] double as_number(double fallback = 0.0) const noexcept {
        return type_ == Type::Number ? number_value_ : fallback;
    }
    [[nodiscard]] i64 as_int64(i64 fallback = 0) const noexcept {
        return type_ == Type::Number ? static_cast<i64>(number_value_) : fallback;
    }
    [[nodiscard]] const std::string& as_string() const noexcept { return string_value_; }

    [[nodiscard]] std::size_t size() const noexcept {
        return type_ == Type::Array ? array_.size() : (type_ == Type::Object ? object_.size() : 0);
    }

    const Value& operator[](std::size_t index) const noexcept { return array_[index]; }
    Value& operator[](std::size_t index) noexcept { return array_[index]; }

    [[nodiscard]] const Value* find(std::string_view key) const noexcept;
    Value* find(std::string_view key) noexcept;

    void set(std::string_view key, Value value);
    void push(Value value);

    [[nodiscard]] const std::string& key_at(std::size_t index) const noexcept {
        return object_[index].first;
    }
    [[nodiscard]] const Value& value_at(std::size_t index) const noexcept {
        return object_[index].second;
    }
    [[nodiscard]] Value& value_at(std::size_t index) noexcept { return object_[index].second; }

private:
    Type type_ = Type::Null;
    bool bool_value_ = false;
    double number_value_ = 0.0;
    std::string string_value_;
    std::vector<Value> array_;
    std::vector<std::pair<std::string, Value>> object_;
};

struct ParseError final {
    std::size_t offset = 0;
    std::string message;
};

/// Parses a complete JSON document. On failure `error` describes the problem.
bool parse(std::string_view text, Value& out, ParseError& error);

/// Serializes deterministically: insertion order, 2-space indent, %.9g numbers.
std::string dump(const Value& value, bool pretty = true, int indent = 2);

} // namespace e3d::json
