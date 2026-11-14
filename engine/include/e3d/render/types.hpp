#pragma once

#include "e3d/core/math.hpp"
#include "e3d/core/types.hpp"

#include <cstddef>
#include <vector>

namespace e3d {

enum class Screen : u8 { Top, Bottom };

enum class ShaderId : u8 { UnlitColor };

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

struct MaterialDesc final {
    ShaderId shader = ShaderId::UnlitColor;
    Color tint{1.0f, 1.0f, 1.0f, 1.0f};
};

struct MeshTag;
struct MaterialTag;
struct TextureTag;

using MeshHandle = Handle<MeshTag>;
using MaterialHandle = Handle<MaterialTag>;
using TextureHandle = Handle<TextureTag>;

} // namespace e3d
