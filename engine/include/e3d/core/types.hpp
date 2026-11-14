#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

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

/// Stable hash of a type name. Used as the runtime type id; never serialized.
constexpr u32 hash_name(std::string_view name) noexcept {
    u32 h = 2166136261u;
    for (char c : name) {
        h ^= static_cast<u8>(c);
        h *= 16777619u;
    }
    return h;
}

using TypeId = u32;

/// Dense runtime entity id: low 24 bits are the pool index, high 8 bits a generation.
using EntityId = u32;
inline constexpr EntityId kInvalidEntity = 0;

constexpr u32 entity_index(EntityId id) noexcept { return id & 0x00FFFFFFu; }
constexpr u8 entity_generation(EntityId id) noexcept { return static_cast<u8>(id >> 24); }
constexpr EntityId make_entity_id(u32 index, u8 generation) noexcept {
    return (index & 0x00FFFFFFu) | (static_cast<u32>(generation) << 24);
}

/// Generation-checked resource handle. `index` stays valid until the resource is
/// destroyed; `generation` is bumped on destruction so stale handles are detectable.
template <typename Tag>
struct Handle final {
    static constexpr u32 kInvalid = kInvalidIndex;

    u32 index = kInvalid;
    u32 generation = 0;

    [[nodiscard]] constexpr bool valid() const noexcept { return index != kInvalid; }
    constexpr void reset() noexcept {
        index = kInvalid;
        generation = 0;
    }

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

struct Vec4 final {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;
};

struct Color final {
    f32 r = 1.0f;
    f32 g = 1.0f;
    f32 b = 1.0f;
    f32 a = 1.0f;
};

/// Serialized asset reference (guid only for now; resolution arrives with the asset db).
struct AssetRef final {
    Guid guid;
};

/// Serialized entity reference, resolved after the whole scene has been created.
struct EntityRef final {
    Guid guid;
    EntityId target = kInvalidEntity;

    [[nodiscard]] bool resolved() const noexcept { return target != kInvalidEntity; }
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
constexpr Vec2 operator*(Vec2 v, f32 s) noexcept { return {v.x * s, v.y * s}; }

constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator*(Vec3 v, f32 s) noexcept { return {v.x * s, v.y * s, v.z * s}; }
constexpr Vec3 operator*(f32 s, Vec3 v) noexcept { return v * s; }
constexpr Vec3& operator+=(Vec3& a, Vec3 b) noexcept {
    a = a + b;
    return a;
}

constexpr Vec4 operator+(Vec4 a, Vec4 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
constexpr Vec4 operator*(Vec4 v, f32 s) noexcept { return {v.x * s, v.y * s, v.z * s, v.w * s}; }

} // namespace e3d
