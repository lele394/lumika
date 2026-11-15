#include "project_scripts.hpp"
#include "tetris_game.hpp"

#include <cstdio>

namespace {

TetrisGame* find_game() {
    const e3d::EntityId root = e3d::world().root();
    return root != e3d::kInvalidEntity ? e3d::world().find_component<TetrisGame>(root) : nullptr;
}

} // namespace

std::span<const e3d::PropDesc> HudBinding::properties() {
    static const e3d::PropDesc props[] = {
        E3D_PROP(HudBinding, show_controls),
    };
    return {props, std::size(props)};
}

e3d::UiText* HudBinding::find_text(const char* name) const {
    for (const auto& slot : e3d::world().entities()) {
        if (slot == nullptr || slot->name != name) continue;
        if (e3d::UiText* text = slot->find_component<e3d::UiText>(); text != nullptr) {
            return text;
        }
    }
    return nullptr;
}

void HudBinding::on_start() {
    game = find_game();

    score = find_text("Score");
    level_top = find_text("LevelTop");
    level_bottom = find_text("LevelBottom");
    lines_top = find_text("LinesTop");
    lines_bottom = find_text("LinesBottom");
    combo_top = find_text("ComboTop");
    combo_bottom = find_text("ComboBottom");
    paused = find_text("Paused");
    game_over = find_text("GameOver");
    restart = find_text("Restart");
    controls[0] = find_text("Controls1");
    controls[1] = find_text("Controls2");
    controls[2] = find_text("Controls3");
}

namespace {

void set_text(e3d::UiText* label, const char* value) {
    if (label != nullptr && label->text != value) label->text = value;
}

void set_visible(e3d::UiText* label, bool visible) {
    if (label != nullptr && label->visible != visible) label->visible = visible;
}

} // namespace

void HudBinding::on_update(float /*delta_time*/) {
    if (game == nullptr) return;

    char buffer[64];

    std::snprintf(buffer, sizeof(buffer), "SCORE %06d", game->score);
    set_text(score, buffer);

    std::snprintf(buffer, sizeof(buffer), "LEVEL %d", game->level);
    set_text(level_top, buffer);
    set_text(level_bottom, buffer);

    std::snprintf(buffer, sizeof(buffer), "LINES %d", game->lines);
    set_text(lines_top, buffer);
    set_text(lines_bottom, buffer);

    const bool show_combo = game->combo > 1;
    std::snprintf(buffer, sizeof(buffer), "COMBO x%d", game->combo);
    set_text(combo_top, buffer);
    set_text(combo_bottom, buffer);
    set_visible(combo_top, show_combo);
    set_visible(combo_bottom, show_combo);

    set_visible(paused, game->paused);
    set_visible(game_over, game->game_over);
    set_visible(restart, game->game_over);

    for (e3d::UiText* label : controls) set_visible(label, show_controls);
}

E3D_IMPLEMENT_COMPONENT(HudBinding)
