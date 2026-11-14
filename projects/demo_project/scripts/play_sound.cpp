#include "project_scripts.hpp"

namespace {

const char* const kButtonNames[] = {"A",  "B",     "X",     "Y",     "L",      "R",
                                    "ZL", "ZR",    "Start", "Select", "Up",     "Down",
                                    "Left", "Right", "Touch", nullptr};

} // namespace

std::span<const e3d::PropDesc> PlaySoundOnButton::properties() {
    static const e3d::PropDesc props[] = {
        E3D_PROP(PlaySoundOnButton, button).enum_names(kButtonNames),
        E3D_PROP(PlaySoundOnButton, volume).range(0.0f, 1.0f).step(0.01f),
    };
    return {props, std::size(props)};
}

void PlaySoundOnButton::on_update(float /*delta_time*/) {
    if (!e3d::input().is_pressed(static_cast<e3d::Button>(button))) return;

    e3d::Entity* owner = e3d::world().entity(entity());
    if (owner == nullptr) return;

    e3d::AudioSource* source = owner->find_component<e3d::AudioSource>();
    if (source == nullptr || !source->clip_handle.valid()) return;

    e3d::PlayParams params;
    params.volume = volume;
    params.loop = source->loop;
    e3d::audio().play(source->clip_handle, params);
}

E3D_IMPLEMENT_COMPONENT(PlaySoundOnButton)
