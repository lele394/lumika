// M3 demo: data-driven scene, asset resolution, scripts, fixed step, input and audio.
// Two cubes spin from scene-defined script components; A plays a WAV from RomFS.

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

namespace {

using namespace e3d;

void write_report(const char* text) {
    FILE* file = std::fopen("sdmc:/3ds-engine-demo.txt", "w");
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
    const bool audio_ready = audio->init();

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

    InputState input;
    RenderList list;

    ScriptContext& context = script_context();
    context.world = &world;
    context.input = &input;
    context.audio = audio.get();
    context.render_list = &list;

    Runtime runtime(world, audio.get());
    runtime.set_render_list(&list);

    u32 frames = 0;
    u64 last_ms = osGetTime();
    const u64 start_ms = last_ms;

    bool audio_voice_ok = false;
    bool audio_playing = false;
    bool audio_clip_ok = false;
    bool report_written = false;

    while (aptMainLoop() && frames < 1800) {
        hidScanInput();
        poll_input_3ds(input);

        if (input.is_pressed(Button::Start)) break;
        if (input.is_pressed(Button::Select)) runtime.set_paused(!runtime.paused());

        const u64 now = osGetTime();
        float dt = static_cast<float>(now - last_ms) / 1000.0f;
        last_ms = now;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;

        runtime.update(input, dt);
        world.update_transforms();
        build_render_list(world, list);
        render->execute(list);

        ++frames;

        if (frames == 60) {
            const EntityId audio_entity = find_entity(world, "Audio");
            AudioSource* source = audio_entity != kInvalidEntity
                                      ? world.find_component<AudioSource>(audio_entity)
                                      : nullptr;
            if (source != nullptr && source->clip_handle.valid()) {
                audio_clip_ok = true;
                const VoiceHandle voice =
                    audio->play(source->clip_handle, PlayParams{0.4f, 0.0f, 1.0f, false});
                audio_voice_ok = voice.valid();
                audio_playing = audio->is_playing(voice);
            }
        }

        if (!report_written && frames == 120) {
            const u64 elapsed = osGetTime() - start_ms;
            unsigned camera_draws[2] = {0, 0};
            for (const DrawItem& item : list.draws()) {
                if (item.camera < 2) ++camera_draws[item.camera];
            }

            const EntityId top_cube = find_entity(world, "TopCube");
            const MeshRenderer* top_renderer =
                top_cube != kInvalidEntity ? world.find_component<MeshRenderer>(top_cube) : nullptr;
            const Entity* top_entity = world.entity(top_cube);
            const float top_rotation = top_entity != nullptr ? top_entity->transform.rotation.y : 0.0f;

            char report[1024];
            std::snprintf(report, sizeof(report),
                          "scene=%s\n"
                          "warnings=%u\n"
                          "scripts=%u\n"
                          "manifest=%d\n"
                          "mesh_ok=%d\n"
                          "material_ok=%d\n"
                          "audio_device=%d\n"
                          "audio_clip=%d\n"
                          "audio_voice=%d\n"
                          "audio_playing=%d\n"
                          "audio_result=0x%08lx\n"
                          "top_rotation_milli=%d\n"
                          "frames=%u\n"
                          "elapsed_ms=%llu\n"
                          "us_per_frame=%llu\n"
                          "draws=%u\n"
                          "top_draws=%u\n"
                          "bottom_draws=%u\n",
                          loaded.ok ? "ok" : "failed",
                          static_cast<unsigned>(loaded.warnings.size() + warnings.size()),
                          count_scripts(world), manifest_ok ? 1 : 0,
                          top_renderer != nullptr && top_renderer->mesh_handle.valid() ? 1 : 0,
                          top_renderer != nullptr && top_renderer->material_handle.valid() ? 1 : 0,
                          audio_ready ? 1 : 0, audio_clip_ok ? 1 : 0, audio_voice_ok ? 1 : 0,
                          audio_playing ? 1 : 0,
                          static_cast<unsigned long>(audio_device_3ds_last_result()),
                          static_cast<int>(top_rotation * 1000.0f), static_cast<unsigned>(frames),
                          static_cast<unsigned long long>(elapsed),
                          static_cast<unsigned long long>((elapsed * 1000ull) / frames),
                          static_cast<unsigned>(list.draws().size()), camera_draws[0],
                          camera_draws[1]);
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
