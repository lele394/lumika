#pragma once

#include <cstddef>
#include <cstdint>

namespace e3d {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;

inline constexpr u32 kInvalidIndex = 0xFFFFFFFFu;

/// Generation-checked resource handle. `index` stays valid until the resource is
/// destroyed; `generation` is bumped on destruction so stale handles are detectable.
template <typename Tag>
struct Handle final {
    static constexpr u32 kInvalid = kInvalidIndex;

    u32 index = kInvalid;
    u32 generation = 0;

    [[nodiscard]] constexpr bool valid() const noexcept { return index != kInvalid; }
    constexpr void reset() noexcept { index = kInvalid; generation = 0; }

    constexpr bool operator==(const Handle&) const noexcept = default;
};

/// Persistent identity used in serialized data. Never used as a runtime lookup key.
struct Guid final {
    u64 hi = 0;
    u64 lo = 0;

    [[nodiscard]] constexpr bool valid() const noexcept { return (hi | lo) != 0; }
    constexpr bool operator==(const Guid&) const noexcept = default;
};

struct Vec2 final {
    f32 x = 0.0f;
    f32 y = 0.0f;
};

struct Vec3 final {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
};

struct Color final {
    f32 r = 1.0f;
    f32 g = 1.0f;
    f32 b = 1.0f;
    f32 a = 1.0f;
};

constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator*(Vec3 v, f32 s) noexcept { return {v.x * s, v.y * s, v.z * s}; }
constexpr Vec3 operator*(f32 s, Vec3 v) noexcept { return v * s; }
constexpr Vec3& operator+=(Vec3& a, Vec3 b) noexcept { a = a + b; return a; }

} // namespace e3d
