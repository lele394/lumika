// M2 renderer check: cooked cube mesh + t3x texture, lit shader, two cameras.
// Assets are loaded from RomFS through the real cooked-asset path.

#include <3ds.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "e3d/assets/mesh_loader.hpp"
#include "e3d/e3d.hpp"
#include "e3d/platform/3ds/render_device_3ds.hpp"
#include "e3d/render/renderer.hpp"

namespace {

using namespace e3d;

void write_report(const char* text) {
    FILE* file = std::fopen("sdmc:/3ds-engine-render-check.txt", "w");
    if (file == nullptr) return;
    std::fputs(text, file);
    std::fclose(file);
}

std::vector<u8> read_file(const char* path) {
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr) return {};

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(file);
        return {};
    }

    std::vector<u8> data(static_cast<std::size_t>(size));
    const std::size_t read = std::fread(data.data(), 1, data.size(), file);
    std::fclose(file);
    if (read != data.size()) return {};
    return data;
}

} // namespace

int main() {
    const bool romfs_ready = R_SUCCEEDED(romfsInit());

    std::unique_ptr<RenderDevice> device = create_render_device_3ds();
    if (!device->init()) {
        write_report("init_failed\n");
        return 1;
    }

    std::string mesh_error;
    MeshData cube;
    bool mesh_ok = false;

    if (romfs_ready) {
        const std::vector<u8> mesh_bytes = read_file("romfs:/models/cube.cmesh");
        if (!mesh_bytes.empty()) {
            cube = load_cmesh(mesh_bytes.data(), mesh_bytes.size(), mesh_error);
            mesh_ok = !cube.vertices.empty();
        } else {
            mesh_error = "cube.cmesh not found";
        }
    }

    TextureHandle texture;
    bool texture_ok = false;
    if (romfs_ready) {
        const std::vector<u8> tex_bytes = read_file("romfs:/textures/checker.t3x");
        if (!tex_bytes.empty()) {
            texture = device->create_texture(TextureData{tex_bytes.data(), tex_bytes.size()});
            texture_ok = texture.valid();
        }
    }

    if (!mesh_ok) {
        char report[512];
        std::snprintf(report, sizeof(report), "mesh_failed\n%s\n", mesh_error.c_str());
        write_report(report);
        device->shutdown();
        return 1;
    }

    const MeshHandle cube_mesh = device->create_mesh(cube);

    MaterialDesc material;
    material.shader = ShaderId::LitTextured;
    material.tint = Color{1.0f, 1.0f, 1.0f, 1.0f};
    material.ambient = 0.22f;
    material.diffuse = 0.85f;
    material.light_direction = Vec3{-0.45f, 0.75f, 0.5f};
    material.texture = texture;
    const MaterialHandle cube_material = device->create_material(material);

    World world;
    const EntityId root = world.create_entity("Root");
    Environment* environment = world.add_component<Environment>(root);
    environment->clear_color = Color{0.02f, 0.03f, 0.07f, 1.0f};

    const EntityId top_camera_entity = world.create_entity("TopCamera", root);
    world.entity(top_camera_entity)->transform.position = Vec3{0.0f, 0.0f, 3.6f};
    Camera* top_camera = world.add_component<Camera>(top_camera_entity);
    top_camera->screen = ScreenTarget::Top;
    top_camera->field_of_view = 55.0f;
    top_camera->cull_mask = 1;

    const EntityId bottom_camera_entity = world.create_entity("BottomCamera", root);
    world.entity(bottom_camera_entity)->transform.position = Vec3{0.0f, 0.0f, 3.0f};
    Camera* bottom_camera = world.add_component<Camera>(bottom_camera_entity);
    bottom_camera->screen = ScreenTarget::Bottom;
    bottom_camera->field_of_view = 70.0f;
    bottom_camera->cull_mask = 2;

    const EntityId top_cube = world.create_entity("TopCube", root);
    world.entity(top_cube)->transform.scale = Vec3{1.3f, 1.3f, 1.3f};
    MeshRenderer* top_renderer = world.add_component<MeshRenderer>(top_cube);
    top_renderer->mesh_handle = cube_mesh;
    top_renderer->material_handle = cube_material;
    top_renderer->layer = 0;

    const EntityId bottom_cube = world.create_entity("BottomCube", root);
    MeshRenderer* bottom_renderer = world.add_component<MeshRenderer>(bottom_cube);
    bottom_renderer->mesh_handle = cube_mesh;
    bottom_renderer->material_handle = cube_material;
    bottom_renderer->layer = 1;

    RenderList list;
    u32 frames = 0;
    float top_angle = 0.0f;
    float bottom_angle = 0.0f;
    bool report_written = false;

    const u64 start_ms = osGetTime();
    u64 last_ms = start_ms;

    while (aptMainLoop() && frames < 1800) {
        hidScanInput();
        if ((hidKeysDown() & KEY_START) != 0) break;

        const u64 now = osGetTime();
        float dt = static_cast<float>(now - last_ms) / 1000.0f;
        last_ms = now;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        top_angle += dt * 55.0f;
        bottom_angle -= dt * 95.0f;

        world.entity(top_cube)->transform.rotation = Vec3{top_angle * 0.35f, top_angle, 0.0f};
        world.entity(bottom_cube)->transform.rotation =
            Vec3{0.0f, bottom_angle, bottom_angle * 0.4f};

        world.update_transforms();
        build_render_list(world, list);
        device->execute(list);

        ++frames;

        if (!report_written && frames == 120) {
            const u64 elapsed = osGetTime() - start_ms;
            unsigned camera_draws[2] = {0, 0};
            for (const DrawItem& item : list.draws()) {
                if (item.camera < 2) ++camera_draws[item.camera];
            }
            char report[640];
            std::snprintf(report, sizeof(report),
                          "mesh=%s\n"
                          "texture=%s\n"
                          "vertices=%u\n"
                          "indices=%u\n"
                          "frames=%u\n"
                          "elapsed_ms=%llu\n"
                          "us_per_frame=%llu\n"
                          "draws=%u\n"
                          "cameras=%u\n"
                          "top_draws=%u\n"
                          "bottom_draws=%u\n",
                          mesh_ok ? "ok" : "failed", texture_ok ? "ok" : "failed",
                          static_cast<unsigned>(cube.vertices.size()),
                          static_cast<unsigned>(cube.indices.size()),
                          static_cast<unsigned>(frames),
                          static_cast<unsigned long long>(elapsed),
                          static_cast<unsigned long long>((elapsed * 1000ull) / frames),
                          static_cast<unsigned>(list.draws().size()),
                          static_cast<unsigned>(list.cameras().size()), camera_draws[0],
                          camera_draws[1]);
            write_report(report);
            report_written = true;
        }
    }

    const u64 total_ms = osGetTime() - start_ms;
    char report[640];
    std::snprintf(report, sizeof(report),
                  "mesh=%s\n"
                  "texture=%s\n"
                  "vertices=%u\n"
                  "indices=%u\n"
                  "frames=%u\n"
                  "elapsed_ms=%llu\n"
                  "us_per_frame=%llu\n"
                  "draws=%u\n"
                  "cameras=%u\n",
                  mesh_ok ? "ok" : "failed", texture_ok ? "ok" : "failed",
                  static_cast<unsigned>(cube.vertices.size()),
                  static_cast<unsigned>(cube.indices.size()), static_cast<unsigned>(frames),
                  static_cast<unsigned long long>(total_ms),
                  static_cast<unsigned long long>(frames > 0 ? (total_ms * 1000ull) / frames : 0),
                  static_cast<unsigned>(list.draws().size()),
                  static_cast<unsigned>(list.cameras().size()));
    write_report(report);

    device->shutdown();
    if (romfs_ready) romfsExit();
    return 0;
}
