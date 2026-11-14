// Tetris demo: scene-driven board, next piece, HUD and game rules.

#include <3ds.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "e3d/e3d.hpp"
#include "e3d/platform/3ds/audio_device_3ds.hpp"
#include "e3d/platform/3ds/file_system_3ds.hpp"
#include "e3d/platform/3ds/render_device_3ds.hpp"
#include "project_scripts.hpp"
#include "tetris_game.hpp"

namespace {

using namespace e3d;

void write_report(const char* text) {
    FILE* file = std::fopen("sdmc:/3ds-engine-tetris.txt", "w");
    if (file == nullptr) return;
    std::fputs(text, file);
    std::fclose(file);
}

std::string read_text(FileSystem& files, const char* path) {
    std::vector<u8> bytes;
    if (!files.read(path, bytes)) return {};
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

EntityId find_entity(World& world, const char* name) {
    for (const auto& slot : world.entities()) {
        if (slot != nullptr && slot->name == name) return slot->id;
    }
    return kInvalidEntity;
}

unsigned count_scripts(World& world) {
    unsigned count = 0;
    for (const auto& slot : world.entities()) {
        if (slot == nullptr) continue;
        for (const Component* component : slot->components) {
            if (component->descriptor()->kind == ComponentKind::Script) ++count;
        }
    }
    return count;
}

} // namespace

int main() {
    const bool romfs_ready = R_SUCCEEDED(romfsInit());
    register_project_components();

    std::unique_ptr<AudioDevice> audio = create_audio_device_3ds();
    audio->init();

    std::unique_ptr<RenderDevice> render = create_render_device_3ds();
    if (!render->init()) {
        write_report("render_init_failed\n");
        return 1;
    }

    RomFsFileSystem files;
    AssetDb database;
    std::string error;
    const std::string manifest = read_text(files, "manifest.json");
    const bool manifest_ok = !manifest.empty() && database.load_manifest(manifest, error);

    AssetManager assets(database, files, *render, audio.get());

    World world;
    const std::string scene_text = read_text(files, "main.scene.json");
    const SceneLoadResult loaded = load_scene(world, scene_text);

    std::vector<std::string> warnings;
    if (loaded.ok) resolve_scene_assets(world, assets, warnings);

    Font font;
    {
        Guid guid;
        guid_from_string("a1b2c3d4-e5f6-4a7b-8c9d-0e1f2a3b4c5d", guid);
        font.texture = assets.texture(guid, error);
        font.texture_width = 128;
        font.texture_height = 128;
        font.glyph_width = 8;
        font.glyph_height = 12;
        font.columns = 16;
        font.first_char = 32;
    }

    TetrisHud* hud = nullptr;
    if (const EntityId hud_entity = find_entity(world, "Hud"); hud_entity != kInvalidEntity) {
        hud = world.find_component<TetrisHud>(hud_entity);
        if (hud != nullptr) {
            hud->font = font;
            hud->has_font = font.texture.valid();
        }
    }

    InputState input;
    RenderList list;

    ScriptContext& context = script_context();
    context.world = &world;
    context.input = &input;
    context.audio = audio.get();
    context.render_list = &list;

    Runtime runtime(world, audio.get());
    runtime.set_render_list(&list);
    runtime.set_render_device(render.get());

    const EntityId root = world.root();
    TetrisGame* game = root != kInvalidEntity ? world.find_component<TetrisGame>(root) : nullptr;

    u32 frames = 0;
    u64 last_ms = osGetTime();
    const u64 start_ms = last_ms;
    bool report_written = false;

    while (aptMainLoop() && frames < 3600) {
        hidScanInput();
        poll_input_3ds(input);

        if (input.is_pressed(Button::Select)) break;

        const u64 now = osGetTime();
        float dt = static_cast<float>(now - last_ms) / 1000.0f;
        last_ms = now;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;

        runtime.update(input, dt);
        world.update_transforms();
        build_render_list(world, list);
        render_scripts(world, list);
        render->execute(list);

        ++frames;

        if (!report_written && frames == 120) {
            const u64 elapsed = osGetTime() - start_ms;
            unsigned camera_draws[2] = {0, 0};
            unsigned cam_top = 0;
            unsigned cam_bottom = 0;
            for (const DrawItem& item : list.draws()) {
                if (item.camera < 2) ++camera_draws[item.camera];
            }
            for (const CameraDesc& camera : list.cameras()) {
                if (camera.screen == Screen::Top) ++cam_top;
                else ++cam_bottom;
            }

            const BoardView* board_view = nullptr;
            if (const EntityId board = find_entity(world, "Board"); board != kInvalidEntity) {
                board_view = world.find_component<BoardView>(board);
            }

            unsigned cells = 0;
            if (game != nullptr) {
                for (int i = 0; i < game->width * game->height; ++i) {
                    if (game->board[i] != 0) ++cells;
                }
            }

            const MeshRenderer* board_renderer = nullptr;
            if (const EntityId board = find_entity(world, "Board"); board != kInvalidEntity) {
                board_renderer = world.find_component<MeshRenderer>(board);
            }
            const MeshRenderer* next_renderer = nullptr;
            if (const EntityId next = find_entity(world, "Next"); next != kInvalidEntity) {
                next_renderer = world.find_component<MeshRenderer>(next);
            }

            char report[1200];
            std::snprintf(report, sizeof(report),
                          "scene=%s\n"
                          "warnings=%u\n"
                          "scripts=%u\n"
                          "manifest=%d\n"
                          "font_tex=%d\n"
                          "board_mesh=%d\n"
                          "next_mesh=%d\n"
                          "score=%d\n"
                          "level=%d\n"
                          "lines=%d\n"
                          "locked=%d\n"
                          "cells=%u\n"
                          "revision=%u\n"
                          "game_over=%d\n"
                          "paused=%d\n"
                          "frames=%u\n"
                          "elapsed_ms=%llu\n"
                          "us_per_frame=%llu\n"
                          "draws=%u\n"
                          "top_draws=%u\n"
                          "bottom_draws=%u\n"
                          "cam_top=%u\n"
                          "cam_bottom=%u\n"
                          "board_vertices=%d\n"
                          "board_update_ok=%d\n",
                          loaded.ok ? "ok" : "failed",
                          static_cast<unsigned>(loaded.warnings.size() + warnings.size()),
                          count_scripts(world), manifest_ok ? 1 : 0, font.texture.valid() ? 1 : 0,
                          board_renderer != nullptr && board_renderer->mesh_handle.valid() ? 1 : 0,
                          next_renderer != nullptr && next_renderer->mesh_handle.valid() ? 1 : 0,
                          game != nullptr ? game->score : -1, game != nullptr ? game->level : -1,
                          game != nullptr ? game->lines : -1,
                          game != nullptr ? game->pieces_locked : -1, cells,
                          game != nullptr ? game->revision : 0u,
                          game != nullptr && game->game_over ? 1 : 0,
                          game != nullptr && game->paused ? 1 : 0, static_cast<unsigned>(frames),
                          static_cast<unsigned long long>(elapsed),
                          static_cast<unsigned long long>((elapsed * 1000ull) / frames),
                          static_cast<unsigned>(list.draws().size()), camera_draws[0],
                          camera_draws[1], cam_top, cam_bottom,
                          board_view != nullptr ? board_view->mesh_vertices : -1,
                          board_view != nullptr && board_view->update_ok ? 1 : 0);
            write_report(report);
            report_written = true;
        }
    }

    if (!loaded.ok) {
        char report[1024];
        std::snprintf(report, sizeof(report), "scene=failed\n%s\n%s\n", loaded.error.c_str(),
                      error.c_str());
        write_report(report);
    }

    shutdown_scripts(world);
    audio->shutdown();
    render->shutdown();
    if (romfs_ready) romfsExit();
    return 0;
}
