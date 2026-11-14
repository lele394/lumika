#pragma once

#include <e3d/e3d.hpp>

#include <span>

class TetrisGame final : public e3d::ScriptComponent {
public:
    E3D_COMPONENT(TetrisGame, e3d::ScriptComponent)

    int width = 10;
    int height = 20;
    float base_gravity = 1.0f;
    bool use_seven_bag = true;

    // Runtime state (not serialized).
    e3d::u8 board[200] = {};
    int score = 0;
    int level = 1;
    int lines = 0;
    int pieces_locked = 0;
    int combo = 0;
    bool game_over = false;
    bool paused = false;
    e3d::u32 revision = 0;

    int next_type = 0;
    int piece_type = 0;
    int piece_rot = 0;
    int piece_x = 3;
    int piece_y = 0;

    // Runtime audio handles, assigned by the application.
    e3d::ClipHandle clip_lock;

    static std::span<const e3d::PropDesc> properties();

    void on_create() override;
    void on_update(float delta_time) override;

    void reset();

    [[nodiscard]] e3d::u8 cell(int x, int y) const;
    [[nodiscard]] bool piece_cell(int x, int y) const;
    [[nodiscard]] static e3d::u16 shape_mask(int type, int rotation);
    [[nodiscard]] static e3d::Color color_for(int type);

private:
    float gravity_timer_ = 0.0f;
    float lock_timer_ = 0.0f;
    float repeat_timer_ = 0.0f;
    int repeat_dir_ = 0;
    int bag_[7] = {};
    int bag_index_ = 7;
    e3d::u32 random_state_ = 0x12345u;

    bool try_move(int dx, int dy);
    void rotate(int direction);
    void hard_drop();
    void lock_piece();
    void clear_lines();
    bool spawn();
    int draw_type();
    [[nodiscard]] float gravity_interval() const;
    [[nodiscard]] bool collide(int type, int rotation, int x, int y) const;
};
