#include "project_scripts.hpp"
#include "cube_mesh.hpp"
#include "tetris_game.hpp"

namespace {

TetrisGame* find_game() {
    const e3d::EntityId root = e3d::world().root();
    return root != e3d::kInvalidEntity ? e3d::world().find_component<TetrisGame>(root) : nullptr;
}

e3d::MeshHandle find_mesh(e3d::EntityId entity) {
    e3d::Entity* owner = e3d::world().entity(entity);
    if (owner == nullptr) return {};
    e3d::MeshRenderer* renderer = owner->find_component<e3d::MeshRenderer>();
    return renderer != nullptr ? renderer->mesh_handle : e3d::MeshHandle{};
}

} // namespace

std::span<const e3d::PropDesc> BoardView::properties() {
    static const e3d::PropDesc props[] = {
        E3D_PROP(BoardView, cube_size).range(0.5f, 1.0f).step(0.01f),
    };
    return {props, std::size(props)};
}

void BoardView::on_start() {
    handle = find_mesh(entity());
    game = find_game();
    last_revision = 0xFFFFFFFFu;
}

void BoardView::on_update(float /*delta_time*/) {
    if (game == nullptr || !handle.valid()) return;
    if (game->revision == last_revision) return;
    last_revision = game->revision;

    e3d::MeshData mesh;
    mesh.vertices.reserve(240 * 24);
    mesh.indices.reserve(240 * 36);

    const float half_width = static_cast<float>(game->width) * 0.5f;
    const float half_height = static_cast<float>(game->height) * 0.5f;

    // Playfield panel behind the cubes.
    {
        const float x0 = -half_width - 0.35f;
        const float x1 = half_width + 0.35f;
        const float y0 = -half_height - 0.35f;
        const float y1 = half_height + 0.35f;
        const e3d::Color panel{0.035f, 0.045f, 0.10f, 1.0f};
        const e3d::Vec3 normal{0.0f, 0.0f, 1.0f};
        const e3d::u16 base = static_cast<e3d::u16>(mesh.vertices.size());

        mesh.vertices.push_back(e3d::MeshVertex{e3d::Vec3{x0, y0, -0.6f}, panel,
                                                e3d::Vec2{0, 0}, normal});
        mesh.vertices.push_back(e3d::MeshVertex{e3d::Vec3{x1, y0, -0.6f}, panel,
                                                e3d::Vec2{0, 0}, normal});
        mesh.vertices.push_back(e3d::MeshVertex{e3d::Vec3{x1, y1, -0.6f}, panel,
                                                e3d::Vec2{0, 0}, normal});
        mesh.vertices.push_back(e3d::MeshVertex{e3d::Vec3{x0, y1, -0.6f}, panel,
                                                e3d::Vec2{0, 0}, normal});

        const e3d::u16 indices[6] = {base, static_cast<e3d::u16>(base + 1),
                                     static_cast<e3d::u16>(base + 2), base,
                                     static_cast<e3d::u16>(base + 2),
                                     static_cast<e3d::u16>(base + 3)};
        for (e3d::u16 index : indices) mesh.indices.push_back(index);
    }

    for (int y = 0; y < game->height; ++y) {
        for (int x = 0; x < game->width; ++x) {
            const e3d::u8 cell = game->cell(x, y);
            if (cell == 0) continue;
            const e3d::Vec3 center{static_cast<float>(x) - half_width + 0.5f,
                                   half_height - static_cast<float>(y) - 0.5f, 0.0f};
            demo::append_cube(mesh, center, cube_size, TetrisGame::color_for(cell - 1));
        }
    }

    for (int y = 0; y < game->height; ++y) {
        for (int x = 0; x < game->width; ++x) {
            if (!game->piece_cell(x, y)) continue;
            const e3d::Vec3 center{static_cast<float>(x) - half_width + 0.5f,
                                   half_height - static_cast<float>(y) - 0.5f, 0.0f};
            demo::append_cube(mesh, center, cube_size,
                              TetrisGame::color_for(game->piece_type));
        }
    }

    e3d::render_device().update_mesh(handle, mesh);
    mesh_vertices = static_cast<int>(mesh.vertices.size());
    update_ok = true;
}

E3D_IMPLEMENT_COMPONENT(BoardView)
