#include "tetris_game.hpp"

#include <cstdio>
#include <cstring>

namespace {

constexpr e3d::u16 kBaseShapes[7] = {
    0x0F00,  // I
    0x0066,  // O
    0x0072,  // T
    0x0036,  // S
    0x0063,  // Z
    0x0071,  // J
    0x0074,  // L
};

e3d::u16 g_shapes[7][4];
bool g_shapes_ready = false;

e3d::u16 rotate_mask(e3d::u16 mask) {
    e3d::u16 out = 0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if ((mask & (1u << (y * 4 + x))) != 0) {
                out |= static_cast<e3d::u16>(1u << (x * 4 + (3 - y)));
            }
        }
    }
    return out;
}

void ensure_shapes() {
    if (g_shapes_ready) return;
    g_shapes_ready = true;
    for (int type = 0; type < 7; ++type) {
        g_shapes[type][0] = kBaseShapes[type];
        for (int rotation = 1; rotation < 4; ++rotation) {
            g_shapes[type][rotation] = rotate_mask(g_shapes[type][rotation - 1]);
        }
    }
}

} // namespace

std::span<const e3d::PropDesc> TetrisGame::properties() {
    using namespace e3d;
    static const PropDesc props[] = {
        E3D_PROP(TetrisGame, width).range(10.0f, 10.0f).step(1.0f),
        E3D_PROP(TetrisGame, height).range(20.0f, 20.0f).step(1.0f),
        E3D_PROP(TetrisGame, base_gravity).range(0.1f, 1.5f).step(0.01f),
        E3D_PROP(TetrisGame, use_seven_bag),
    };
    return {props, std::size(props)};
}

e3d::u16 TetrisGame::shape_mask(int type, int rotation) {
    ensure_shapes();
    if (type < 0 || type >= 7) return 0;
    return g_shapes[type][((rotation % 4) + 4) % 4];
}

e3d::Color TetrisGame::color_for(int type) {
    switch (type) {
        case 0: return e3d::Color{0.20f, 0.85f, 0.95f, 1.0f};  // I cyan
        case 1: return e3d::Color{0.98f, 0.85f, 0.20f, 1.0f};  // O yellow
        case 2: return e3d::Color{0.75f, 0.35f, 0.95f, 1.0f};  // T purple
        case 3: return e3d::Color{0.30f, 0.90f, 0.40f, 1.0f};  // S green
        case 4: return e3d::Color{0.95f, 0.30f, 0.30f, 1.0f};  // Z red
        case 5: return e3d::Color{0.30f, 0.50f, 0.98f, 1.0f};  // J blue
        case 6: return e3d::Color{0.98f, 0.60f, 0.20f, 1.0f};  // L orange
        default: return e3d::Color{1.0f, 1.0f, 1.0f, 1.0f};
    }
}

void TetrisGame::on_create() { reset(); }

void TetrisGame::reset() {
    ensure_shapes();
    std::memset(board, 0, sizeof(board));
    score = 0;
    level = 1;
    lines = 0;
    pieces_locked = 0;
    game_over = false;
    paused = false;
    gravity_timer_ = 0.0f;
    lock_timer_ = 0.0f;
    repeat_timer_ = 0.0f;
    repeat_dir_ = 0;
    bag_index_ = 7;
    random_state_ = 0x12345u;

    piece_type = draw_type();
    next_type = draw_type();
    piece_rot = 0;
    piece_x = width / 2 - 2;
    piece_y = 0;
    ++revision;
}

int TetrisGame::draw_type() {
    if (!use_seven_bag) {
        random_state_ = random_state_ * 1664525u + 1013904223u;
        return static_cast<int>((random_state_ >> 16) % 7u);
    }

    if (bag_index_ >= 7) {
        for (int i = 0; i < 7; ++i) bag_[i] = i;
        for (int i = 6; i > 0; --i) {
            random_state_ = random_state_ * 1664525u + 1013904223u;
            const int j = static_cast<int>((random_state_ >> 16) % static_cast<unsigned>(i + 1));
            const int temp = bag_[i];
            bag_[i] = bag_[j];
            bag_[j] = temp;
        }
        bag_index_ = 0;
    }

    return bag_[bag_index_++];
}

e3d::u8 TetrisGame::cell(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return 0;
    return board[y * width + x];
}

bool TetrisGame::piece_cell(int x, int y) const {
    const e3d::u16 mask = shape_mask(piece_type, piece_rot);
    const int sx = x - piece_x;
    const int sy = y - piece_y;
    if (sx < 0 || sx >= 4 || sy < 0 || sy >= 4) return false;
    return (mask & (1u << (sy * 4 + sx))) != 0;
}

bool TetrisGame::collide(int type, int rotation, int x, int y) const {
    const e3d::u16 mask = shape_mask(type, rotation);
    for (int sy = 0; sy < 4; ++sy) {
        for (int sx = 0; sx < 4; ++sx) {
            if ((mask & (1u << (sy * 4 + sx))) == 0) continue;
            const int bx = x + sx;
            const int by = y + sy;
            if (bx < 0 || bx >= width || by >= height) return true;
            if (by >= 0 && board[by * width + bx] != 0) return true;
        }
    }
    return false;
}

bool TetrisGame::try_move(int dx, int dy) {
    if (collide(piece_type, piece_rot, piece_x + dx, piece_y + dy)) return false;
    piece_x += dx;
    piece_y += dy;
    lock_timer_ = 0.0f;
    ++revision;
    return true;
}

void TetrisGame::rotate(int direction) {
    const int next = (piece_rot + (direction > 0 ? 1 : 3)) % 4;
    const int kicks[4][2] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}};

    for (const auto& kick : kicks) {
        if (!collide(piece_type, next, piece_x + kick[0], piece_y + kick[1])) {
            piece_rot = next;
            piece_x += kick[0];
            piece_y += kick[1];
            lock_timer_ = 0.0f;
            ++revision;
            return;
        }
    }
}

void TetrisGame::hard_drop() {
    int dropped = 0;
    while (try_move(0, 1)) ++dropped;
    score += dropped * 2;
    lock_piece();
}

void TetrisGame::lock_piece() {
    const e3d::u16 mask = shape_mask(piece_type, piece_rot);
    for (int sy = 0; sy < 4; ++sy) {
        for (int sx = 0; sx < 4; ++sx) {
            if ((mask & (1u << (sy * 4 + sx))) == 0) continue;
            const int bx = piece_x + sx;
            const int by = piece_y + sy;
            if (bx >= 0 && bx < width && by >= 0 && by < height) {
                board[by * width + bx] = static_cast<e3d::u8>(piece_type + 1);
            }
        }
    }

    ++pieces_locked;
    clear_lines();
    if (!spawn()) game_over = true;
    ++revision;

    if (clip_lock.valid()) {
        e3d::audio().play(clip_lock, e3d::PlayParams{0.55f, 0.0f, 1.0f, false});
    }
}

void TetrisGame::clear_lines() {
    static const int kScores[5] = {0, 100, 300, 500, 800};

    int cleared = 0;
    for (int y = height - 1; y >= 0; --y) {
        bool full = true;
        for (int x = 0; x < width; ++x) {
            if (board[y * width + x] == 0) {
                full = false;
                break;
            }
        }
        if (!full) continue;

        for (int row = y; row > 0; --row) {
            std::memcpy(&board[row * width], &board[(row - 1) * width],
                        static_cast<std::size_t>(width));
        }
        std::memset(&board[0], 0, static_cast<std::size_t>(width));
        ++cleared;
        ++y;
    }

    if (cleared > 0) {
        if (cleared > 4) cleared = 4;
        score += kScores[cleared] * level;
        lines += cleared;
        level = 1 + lines / 10;
    }
}

bool TetrisGame::spawn() {
    piece_type = next_type;
    next_type = draw_type();
    piece_rot = 0;
    piece_x = width / 2 - 2;
    piece_y = 0;
    gravity_timer_ = 0.0f;
    lock_timer_ = 0.0f;
    return !collide(piece_type, piece_rot, piece_x, piece_y);
}

float TetrisGame::gravity_interval() const {
    float interval = base_gravity - static_cast<float>(level - 1) * 0.05f;
    if (interval < 0.08f) interval = 0.08f;
    return interval;
}

void TetrisGame::on_update(float delta_time) {
    using namespace e3d;

    if (game_over) {
        if (input().is_pressed(Button::A)) reset();
        return;
    }

    if (input().is_pressed(Button::Start)) {
        paused = !paused;
        ++revision;
        return;
    }
    if (paused) return;

    if (input().is_pressed(Button::A)) rotate(1);
    if (input().is_pressed(Button::B)) rotate(-1);
    if (input().is_pressed(Button::Up)) {
        hard_drop();
        return;
    }

    int direction = 0;
    if (input().is_pressed(Button::Left)) direction = -1;
    else if (input().is_pressed(Button::Right)) direction = 1;

    if (direction != 0) {
        repeat_dir_ = direction;
        repeat_timer_ = 0.16f;
        try_move(direction, 0);
    } else if (repeat_dir_ != 0) {
        const Button held = repeat_dir_ < 0 ? Button::Left : Button::Right;
        if (!input().is_held(held)) {
            repeat_dir_ = 0;
        } else {
            repeat_timer_ -= delta_time;
            if (repeat_timer_ <= 0.0f) {
                repeat_timer_ = 0.06f;
                try_move(repeat_dir_, 0);
            }
        }
    }

    const bool soft_drop = input().is_held(Button::Down);
    gravity_timer_ += delta_time * (soft_drop ? 10.0f : 1.0f);

    if (gravity_timer_ >= gravity_interval()) {
        gravity_timer_ = 0.0f;
        if (!try_move(0, 1)) {
            lock_timer_ += delta_time;
            if (lock_timer_ >= 0.35f || soft_drop) lock_piece();
        } else {
            lock_timer_ = 0.0f;
        }
    }
}

E3D_IMPLEMENT_COMPONENT(TetrisGame)
