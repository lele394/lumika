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

std::span<const PropDesc> UiCanvas::properties() {
    static const PropDesc props[] = {
        E3D_PROP(UiCanvas, screen).enum_names(kScreenNames),
        E3D_PROP(UiCanvas, font).asset_type("texture"),
        E3D_PROP(UiCanvas, glyph_width).range(1.0f, 64.0f).step(1.0f),
        E3D_PROP(UiCanvas, glyph_height).range(1.0f, 64.0f).step(1.0f),
        E3D_PROP(UiCanvas, columns).range(1.0f, 64.0f).step(1.0f),
        E3D_PROP(UiCanvas, first_char).range(1.0f, 255.0f).step(1.0f),
        E3D_PROP(UiCanvas, atlas_width).range(8.0f, 1024.0f).step(1.0f),
        E3D_PROP(UiCanvas, atlas_height).range(8.0f, 1024.0f).step(1.0f),
    };
    return {props, std::size(props)};
}

std::span<const PropDesc> UiText::properties() {
    static const PropDesc props[] = {
        E3D_PROP(UiText, text),
        E3D_PROP(UiText, x).range(-1024.0f, 1024.0f).step(1.0f),
        E3D_PROP(UiText, y).range(-1024.0f, 1024.0f).step(1.0f),
        E3D_PROP(UiText, scale).range(0.25f, 8.0f).step(0.05f),
        E3D_PROP(UiText, color),
        E3D_PROP(UiText, visible),
    };
    return {props, std::size(props)};
}

std::span<const PropDesc> UiImage::properties() {
    static const PropDesc props[] = {
        E3D_PROP(UiImage, texture).asset_type("texture"),
        E3D_PROP(UiImage, x).range(-1024.0f, 1024.0f).step(1.0f),
        E3D_PROP(UiImage, y).range(-1024.0f, 1024.0f).step(1.0f),
        E3D_PROP(UiImage, width).range(1.0f, 1024.0f).step(1.0f),
        E3D_PROP(UiImage, height).range(1.0f, 1024.0f).step(1.0f),
        E3D_PROP(UiImage, color),
        E3D_PROP(UiImage, visible),
    };
    return {props, std::size(props)};
}

E3D_IMPLEMENT_COMPONENT(Environment)
E3D_IMPLEMENT_COMPONENT(Camera)
E3D_IMPLEMENT_COMPONENT(MeshRenderer)
E3D_IMPLEMENT_COMPONENT(AudioSource)
E3D_IMPLEMENT_COMPONENT(UiCanvas)
E3D_IMPLEMENT_COMPONENT(UiText)
E3D_IMPLEMENT_COMPONENT(UiImage)

void register_builtin_components() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    register_component(Environment::static_descriptor());
    register_component(Camera::static_descriptor());
    register_component(MeshRenderer::static_descriptor());
    register_component(AudioSource::static_descriptor());
    register_component(UiCanvas::static_descriptor());
    register_component(UiText::static_descriptor());
    register_component(UiImage::static_descriptor());
}

} // namespace e3d
