#include "project_scripts.hpp"
#include "tetris_game.hpp"

void register_project_components() {
    e3d::register_component(Spin::static_descriptor());
    e3d::register_component(PlaySoundOnButton::static_descriptor());
    e3d::register_component(TetrisGame::static_descriptor());
    e3d::register_component(BoardView::static_descriptor());
    e3d::register_component(NextPieceView::static_descriptor());
    e3d::register_component(TetrisHud::static_descriptor());
}
