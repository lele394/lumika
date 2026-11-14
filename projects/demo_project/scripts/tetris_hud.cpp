#include "project_scripts.hpp"
#include "tetris_game.hpp"

#include <cstdio>

namespace {

TetrisGame* find_game() {
    const e3d::EntityId root = e3d::world().root();
    return root != e3d::kInvalidEntity ? e3d::world().find_component<TetrisGame>(root) : nullptr;
}

const e3d::Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};

} // namespace

std::span<const e3d::PropDesc> TetrisHud::properties() {
    static const e3d::PropDesc props[] = {
        E3D_PROP(TetrisHud, show_controls),
    };
    return {props, std::size(props)};
}

void TetrisHud::on_start() { game = find_game(); }

void TetrisHud::on_render(e3d::RenderList& list) {
    if (!has_font || game == nullptr) return;

    char buffer[64];

    e3d::draw_text(list, font, e3d::Screen::Top, 344.0f, 8.0f, 1.0f,
                   e3d::Color{0.55f, 0.75f, 1.0f, 1.0f}, "LUMIKA");

    std::snprintf(buffer, sizeof(buffer), "SCORE %06d", game->score);
    e3d::draw_text(list, font, e3d::Screen::Top, 8.0f, 8.0f, 1.0f, kWhite, buffer);

    std::snprintf(buffer, sizeof(buffer), "LEVEL %d", game->level);
    e3d::draw_text(list, font, e3d::Screen::Top, 8.0f, 22.0f, 1.0f, kWhite, buffer);

    std::snprintf(buffer, sizeof(buffer), "LINES %d", game->lines);
    e3d::draw_text(list, font, e3d::Screen::Top, 8.0f, 36.0f, 1.0f, kWhite, buffer);

    if (game->combo > 1) {
        std::snprintf(buffer, sizeof(buffer), "COMBO x%d", game->combo);
        e3d::draw_text(list, font, e3d::Screen::Top, 8.0f, 50.0f, 1.0f,
                       e3d::Color{1.0f, 0.85f, 0.3f, 1.0f}, buffer);
    }

    e3d::draw_text(list, font, e3d::Screen::Bottom, 8.0f, 8.0f, 1.0f, kWhite, "NEXT");
    e3d::draw_text(list, font, e3d::Screen::Bottom, 232.0f, 78.0f, 1.5f,
                   e3d::Color{0.55f, 0.75f, 1.0f, 1.0f}, "NEXT");

    std::snprintf(buffer, sizeof(buffer), "LEVEL %d", game->level);
    e3d::draw_text(list, font, e3d::Screen::Bottom, 8.0f, 28.0f, 1.0f, kWhite, buffer);

    std::snprintf(buffer, sizeof(buffer), "LINES %d", game->lines);
    e3d::draw_text(list, font, e3d::Screen::Bottom, 8.0f, 42.0f, 1.0f, kWhite, buffer);

    if (game->combo > 1) {
        std::snprintf(buffer, sizeof(buffer), "COMBO x%d", game->combo);
        e3d::draw_text(list, font, e3d::Screen::Bottom, 8.0f, 56.0f, 1.0f,
                       e3d::Color{1.0f, 0.85f, 0.3f, 1.0f}, buffer);
    }

    if (show_controls) {
        e3d::draw_text(list, font, e3d::Screen::Bottom, 8.0f, 200.0f, 1.0f, kWhite,
                       "PAD MOVE  A/B ROTATE");
        e3d::draw_text(list, font, e3d::Screen::Bottom, 8.0f, 214.0f, 1.0f, kWhite,
                       "UP DROP  DOWN SOFT");
        e3d::draw_text(list, font, e3d::Screen::Bottom, 8.0f, 228.0f, 1.0f, kWhite,
                       "START PAUSE  SELECT QUIT");
    }

    if (game->paused) {
        e3d::draw_text(list, font, e3d::Screen::Top, 150.0f, 100.0f, 2.0f, kWhite, "PAUSED");
    }

    if (game->game_over) {
        e3d::draw_text(list, font, e3d::Screen::Top, 110.0f, 90.0f, 2.0f, kWhite, "GAME OVER");
        e3d::draw_text(list, font, e3d::Screen::Top, 90.0f, 120.0f, 1.0f, kWhite,
                       "PRESS A TO RESTART");
    }
}

E3D_IMPLEMENT_COMPONENT(TetrisHud)
