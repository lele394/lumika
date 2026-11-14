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

class TetrisGame;

class BoardView final : public e3d::ScriptComponent {
public:
    E3D_COMPONENT(BoardView, e3d::ScriptComponent)

    float cube_size = 0.9f;

    e3d::u32 last_revision = 0xFFFFFFFFu;
    e3d::MeshHandle handle;
    TetrisGame* game = nullptr;
    int mesh_vertices = 0;
    bool update_ok = false;

    static std::span<const e3d::PropDesc> properties();
    void on_start() override;
    void on_update(float delta_time) override;
};

class NextPieceView final : public e3d::ScriptComponent {
public:
    E3D_COMPONENT(NextPieceView, e3d::ScriptComponent)

    float cube_size = 0.9f;
    float spin_speed = 55.0f;

    float angle = 0.0f;
    e3d::MeshHandle handle;
    TetrisGame* game = nullptr;

    static std::span<const e3d::PropDesc> properties();
    void on_start() override;
    void on_update(float delta_time) override;
};

class TetrisHud final : public e3d::ScriptComponent {
public:
    E3D_COMPONENT(TetrisHud, e3d::ScriptComponent)

    bool show_controls = true;

    e3d::Font font;
    bool has_font = false;
    TetrisGame* game = nullptr;

    static std::span<const e3d::PropDesc> properties();
    void on_start() override;
    void on_render(e3d::RenderList& list) override;
};

void register_project_components();
