#pragma once

#include "e3d/core/math.hpp"
#include "e3d/core/types.hpp"

#include <cstddef>
#include <vector>

namespace e3d {

enum class Screen : u8 { Top, Bottom };

enum class ShaderId : u8 { UnlitColor, LitTextured };

struct MeshTag;
struct MaterialTag;
struct TextureTag;

using MeshHandle = Handle<MeshTag>;
using MaterialHandle = Handle<MaterialTag>;
using TextureHandle = Handle<TextureTag>;

/// Fixed vertex layout shared by every engine shader. Attribute order matches the
/// PICA attribute registers: v0 position, v1 colour, v2 uv, v3 normal.
struct MeshVertex final {
    Vec3 position;
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    Vec2 uv{0.0f, 0.0f};
    Vec3 normal{0.0f, 0.0f, 1.0f};
};

static_assert(sizeof(MeshVertex) == 48, "MeshVertex must stay tightly packed for PICA");

struct MeshData final {
    std::vector<MeshVertex> vertices;
    std::vector<u16> indices;
};

/// Raw `.t3x` bytes, as produced by tex3ds and loaded from RomFS.
struct TextureData final {
    const void* data = nullptr;
    std::size_t size = 0;
};

struct MaterialDesc final {
    ShaderId shader = ShaderId::UnlitColor;
    Color tint{1.0f, 1.0f, 1.0f, 1.0f};
    float ambient = 0.25f;
    float diffuse = 0.75f;
    Vec3 light_direction{-0.45f, 0.75f, 0.5f};
    TextureHandle texture;
};

} // namespace e3d
