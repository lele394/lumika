#include <doctest/doctest.h>

#include "e3d/input/input.hpp"
#include "e3d/scene/world.hpp"
#include "e3d/script/script_context.hpp"
#include "tetris_game.hpp"

using namespace e3d;

namespace {

struct Fixture final {
    World world;
    InputState input;
    TetrisGame* game = nullptr;

    Fixture() {
        const EntityId entity = world.create_entity("game");
        game = world.add_component<TetrisGame>(entity);
        script_context().world = &world;
        script_context().input = &input;
        script_context().audio = nullptr;
        game->on_create();
    }

    void press(Button button) {
        input = {};
        input.pressed = button_bit(button);
        game->on_update(1.0f / 60.0f);
    }

    void step(float delta) {
        input = {};
        game->on_update(delta);
    }
};

int count_cells(const TetrisGame& game) {
    int cells = 0;
    for (int i = 0; i < game.width * game.height; ++i) {
        if (game.board[i] != 0) ++cells;
    }
    return cells;
}

} // namespace

TEST_CASE("tetris spawns a piece and responds to movement and rotation") {
    Fixture fixture;
    REQUIRE(fixture.game != nullptr);
    CHECK(fixture.game->piece_type >= 0);
    CHECK(fixture.game->piece_type < 7);
    CHECK_FALSE(fixture.game->game_over);

    const int start_x = fixture.game->piece_x;
    fixture.press(Button::Left);
    CHECK(fixture.game->piece_x == start_x - 1);

    const int rotation = fixture.game->piece_rot;
    fixture.press(Button::A);
    CHECK(fixture.game->piece_rot != rotation);
}

TEST_CASE("tetris hard drop locks a piece and scores") {
    Fixture fixture;
    const int locked = fixture.game->pieces_locked;
    const int score = fixture.game->score;

    fixture.press(Button::Up);

    CHECK(fixture.game->pieces_locked == locked + 1);
    CHECK(count_cells(*fixture.game) >= 4);
    CHECK(fixture.game->score >= score);
}

TEST_CASE("tetris clears a completed line") {
    Fixture fixture;
    fixture.game->reset();

    for (int x = 0; x < 6; ++x) {
        fixture.game->board[19 * fixture.game->width + x] = 1;
    }

    fixture.game->piece_type = 0;  // I
    fixture.game->piece_rot = 0;
    fixture.game->piece_x = 6;
    fixture.game->piece_y = 17;  // I occupies shape row 2, so this lands on board row 19

    fixture.press(Button::Up);

    CHECK(fixture.game->lines == 1);
    CHECK(fixture.game->score >= 100);
    for (int x = 0; x < fixture.game->width; ++x) {
        CHECK(fixture.game->cell(x, 19) == 0);
    }
}

TEST_CASE("tetris gravity moves the piece down") {
    Fixture fixture;
    fixture.game->reset();
    const int start_y = fixture.game->piece_y;

    fixture.step(0.7f);

    CHECK(fixture.game->piece_y > start_y);
}
