#include "e3d/scene/components.hpp"

namespace {

const char* const kScreenNames[] = {"top", "bottom", nullptr};

} // namespace

namespace e3d {

std::span<const PropDesc> Environment::properties() {
    static const PropDesc props[] = {
        E3D_PROP(Environment, clear_color),
        E3D_PROP(Environment, ambient).range(0.0f, 1.0f).step(0.01f),
    };
    return {props, std::size(props)};
}

std::span<const PropDesc> Camera::properties() {
    static const PropDesc props[] = {
        E3D_PROP(Camera, screen).enum_names(kScreenNames),
        E3D_PROP(Camera, field_of_view).range(1.0f, 120.0f).step(0.1f),
        E3D_PROP(Camera, near_plane).range(0.01f, 10.0f).step(0.01f),
        E3D_PROP(Camera, far_plane).range(1.0f, 1000.0f).step(1.0f),
        E3D_PROP(Camera, stereo),
        E3D_PROP(Camera, cull_mask).tooltip("Bitmask of layers this camera renders; -1 = all"),
    };
    return {props, std::size(props)};
}

std::span<const PropDesc> MeshRenderer::properties() {
    static const PropDesc props[] = {
        E3D_PROP(MeshRenderer, mesh).asset_type("mesh"),
        E3D_PROP(MeshRenderer, material).asset_type("material"),
        E3D_PROP(MeshRenderer, layer).range(0.0f, 31.0f).step(1.0f),
    };
    return {props, std::size(props)};
}

std::span<const PropDesc> AudioSource::properties() {
    static const PropDesc props[] = {
        E3D_PROP(AudioSource, clip).asset_type("audio"),
        E3D_PROP(AudioSource, volume).range(0.0f, 1.0f).step(0.01f),
        E3D_PROP(AudioSource, loop),
    };
    return {props, std::size(props)};
}

E3D_IMPLEMENT_COMPONENT(Environment)
E3D_IMPLEMENT_COMPONENT(Camera)
E3D_IMPLEMENT_COMPONENT(MeshRenderer)
E3D_IMPLEMENT_COMPONENT(AudioSource)

void register_builtin_components() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    register_component(Environment::static_descriptor());
    register_component(Camera::static_descriptor());
    register_component(MeshRenderer::static_descriptor());
    register_component(AudioSource::static_descriptor());
}

} // namespace e3d
