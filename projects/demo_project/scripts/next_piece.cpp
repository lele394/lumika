#include "project_scripts.hpp"
#include "cube_mesh.hpp"
#include "tetris_game.hpp"

namespace {

TetrisGame* find_game() {
    const e3d::EntityId root = e3d::world().root();
    return root != e3d::kInvalidEntity ? e3d::world().find_component<TetrisGame>(root) : nullptr;
}

} // namespace

std::span<const e3d::PropDesc> NextPieceView::properties() {
    static const e3d::PropDesc props[] = {
        E3D_PROP(NextPieceView, cube_size).range(0.5f, 1.0f).step(0.01f),
        E3D_PROP(NextPieceView, spin_speed).step(1.0f),
    };
    return {props, std::size(props)};
}

void NextPieceView::on_start() {
    e3d::Entity* owner = e3d::world().entity(entity());
    if (owner != nullptr) {
        e3d::MeshRenderer* renderer = owner->find_component<e3d::MeshRenderer>();
        if (renderer != nullptr) handle = renderer->mesh_handle;
    }
    game = find_game();
}

void NextPieceView::on_update(float delta_time) {
    angle += delta_time * spin_speed;
    if (game == nullptr || !handle.valid()) return;

    const e3d::u16 mask = TetrisGame::shape_mask(game->next_type, 0);

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    int cells = 0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if ((mask & (1u << (y * 4 + x))) == 0) continue;
            sum_x += static_cast<float>(x);
            sum_y += static_cast<float>(y);
            ++cells;
        }
    }
    if (cells == 0) return;

    const float center_x = sum_x / static_cast<float>(cells);
    const float center_y = sum_y / static_cast<float>(cells);
    const e3d::Mat4 rotation =
        e3d::mat4_from_quat(e3d::quat_from_euler_xyz_degrees(e3d::Vec3{0.0f, angle, 0.0f}));

    e3d::MeshData mesh;
    mesh.vertices.reserve(4 * 24);
    mesh.indices.reserve(4 * 36);

    const e3d::Color color = TetrisGame::color_for(game->next_type);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if ((mask & (1u << (y * 4 + x))) == 0) continue;
            const e3d::Vec3 offset{(static_cast<float>(x) - center_x) * 1.05f,
                                   (center_y - static_cast<float>(y)) * 1.05f, 0.0f};
            demo::append_cube(mesh, offset, cube_size, color, rotation);
        }
    }

    e3d::render_device().update_mesh(handle, mesh);
}

E3D_IMPLEMENT_COMPONENT(NextPieceView)
