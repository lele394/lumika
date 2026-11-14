#pragma once

#include "e3d/audio/audio.hpp"
#include "e3d/core/types.hpp"
#include "e3d/reflect/registry.hpp"
#include "e3d/render/types.hpp"

#include <span>

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

/// Registers the engine-provided component types. Idempotent. Called by the scene
/// loader; project scripts register themselves separately.
void register_builtin_components();

} // namespace e3d
