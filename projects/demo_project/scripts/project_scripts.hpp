#pragma once

#include <e3d/e3d.hpp>

#include <span>

class Spin final : public e3d::ScriptComponent {
public:
    E3D_COMPONENT(Spin, e3d::ScriptComponent)

    e3d::Vec3 axis{0.0f, 1.0f, 0.0f};
    float degrees_per_second = 60.0f;

    static std::span<const e3d::PropDesc> properties();
    void on_update(float delta_time) override;
};

class PlaySoundOnButton final : public e3d::ScriptComponent {
public:
    E3D_COMPONENT(PlaySoundOnButton, e3d::ScriptComponent)

    int button = 0;
    float volume = 1.0f;

    static std::span<const e3d::PropDesc> properties();
    void on_update(float delta_time) override;
};

void register_project_components();
