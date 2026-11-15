#pragma once

#include "e3d/audio/audio.hpp"
#include "e3d/core/types.hpp"
#include "e3d/reflect/registry.hpp"
#include "e3d/render/types.hpp"

#include <span>
#include <string>

namespace e3d {

enum class ScreenTarget : i32 { Top = 0, Bottom = 1 };

/// Per-scene rendering environment. Lives on the scene root in the demo project.
class Environment final : public Component {
public:
    E3D_COMPONENT(Environment, Component)

    Color clear_color{0.04f, 0.05f, 0.08f, 1.0f};
    float ambient = 0.25f;

    static std::span<const PropDesc> properties();
};

class Camera final : public Component {
public:
    E3D_COMPONENT(Camera, Component)

    ScreenTarget screen = ScreenTarget::Top;
    float field_of_view = 50.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;
    bool stereo = false;
    int cull_mask = -1;  // -1 = every layer

    static std::span<const PropDesc> properties();
};

class MeshRenderer final : public Component {
public:
    E3D_COMPONENT(MeshRenderer, Component)

    AssetRef mesh;
    AssetRef material;
    int layer = 0;

    // Resolved at runtime; not serialized.
    MeshHandle mesh_handle;
    MaterialHandle material_handle;

    static std::span<const PropDesc> properties();
};

class AudioSource final : public Component {
public:
    E3D_COMPONENT(AudioSource, Component)

    AssetRef clip;
    float volume = 1.0f;
    bool loop = false;

    ClipHandle clip_handle;

    static std::span<const PropDesc> properties();
};

/// Screen-space UI root. Descendant UiText/UiImage components inherit its
/// screen and bitmap font.
class UiCanvas final : public Component {
public:
    E3D_COMPONENT(UiCanvas, Component)

    ScreenTarget screen = ScreenTarget::Top;
    AssetRef font;
    int glyph_width = 8;
    int glyph_height = 12;
    int columns = 16;
    int first_char = 32;
    int atlas_width = 128;
    int atlas_height = 128;

    // Resolved at runtime; not serialized.
    TextureHandle font_texture;

    static std::span<const PropDesc> properties();
};

/// Bitmap text element in UI pixel coordinates (origin top-left).
class UiText final : public Component {
public:
    E3D_COMPONENT(UiText, Component)

    std::string text = "Text";
    float x = 8.0f;
    float y = 8.0f;
    float scale = 1.0f;
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    bool visible = true;

    static std::span<const PropDesc> properties();
};

/// Textured rectangle in UI pixel coordinates.
class UiImage final : public Component {
public:
    E3D_COMPONENT(UiImage, Component)

    AssetRef texture;
    float x = 0.0f;
    float y = 0.0f;
    float width = 64.0f;
    float height = 64.0f;
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    bool visible = true;

    TextureHandle texture_handle;

    static std::span<const PropDesc> properties();
};

/// Registers the engine-provided component types. Idempotent. Called by the scene
/// loader; project scripts register themselves separately.
void register_builtin_components();

} // namespace e3d
