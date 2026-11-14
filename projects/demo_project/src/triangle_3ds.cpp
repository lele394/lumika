// M2/M4 renderer check: two cameras, two independently spinning 3D triangles.
// Loads through the same World -> RenderList -> RenderDevice path the game will use.

#include <3ds.h>

#include <cstdio>
#include <cstring>
#include <memory>

#include "e3d/e3d.hpp"
#include "e3d/platform/3ds/render_device_3ds.hpp"
#include "e3d/render/renderer.hpp"

namespace {

using namespace e3d;

MeshData make_triangle(float size, Color a, Color b, Color c) {
    MeshData mesh;
    mesh.vertices = {
        MeshVertex{Vec3{-size, -size * 0.6f, 0.0f}, a, Vec2{0.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}},
        MeshVertex{Vec3{size, -size * 0.6f, 0.0f}, b, Vec2{1.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}},
        MeshVertex{Vec3{0.0f, size, 0.0f}, c, Vec2{0.5f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}},
    };
    mesh.indices = {0, 1, 2};
    return mesh;
}

void write_report(const char* text) {
    FILE* file = std::fopen("sdmc:/3ds-engine-triangle.txt", "w");
    if (file == nullptr) return;
    std::fputs(text, file);
    std::fclose(file);
}

} // namespace

int main() {
    std::unique_ptr<RenderDevice> device = create_render_device_3ds();
    if (!device->init()) {
        write_report("init_failed\n");
        return 1;
    }

    const MeshHandle top_mesh =
        device->create_mesh(make_triangle(0.9f, Color{1.0f, 0.25f, 0.25f, 1.0f},
                                          Color{0.25f, 1.0f, 0.4f, 1.0f},
                                          Color{0.25f, 0.5f, 1.0f, 1.0f}));
    const MeshHandle bottom_mesh =
        device->create_mesh(make_triangle(0.75f, Color{1.0f, 0.8f, 0.2f, 1.0f},
                                          Color{1.0f, 0.3f, 0.8f, 1.0f},
                                          Color{0.2f, 0.9f, 0.9f, 1.0f}));

    const MaterialHandle top_material =
        device->create_material(MaterialDesc{ShaderId::UnlitColor, Color{1.0f, 1.0f, 1.0f, 1.0f}});
    const MaterialHandle bottom_material =
        device->create_material(MaterialDesc{ShaderId::UnlitColor, Color{0.75f, 0.85f, 1.0f, 1.0f}});

    World world;

    const EntityId root = world.create_entity("Root");
    Environment* environment = world.add_component<Environment>(root);
    environment->clear_color = Color{0.02f, 0.03f, 0.06f, 1.0f};

    const EntityId top_camera_entity = world.create_entity("TopCamera", root);
    world.entity(top_camera_entity)->transform.position = Vec3{0.0f, 0.0f, 4.0f};
    Camera* top_camera = world.add_component<Camera>(top_camera_entity);
    top_camera->screen = ScreenTarget::Top;
    top_camera->field_of_view = 55.0f;
    top_camera->cull_mask = 1;

    const EntityId bottom_camera_entity = world.create_entity("BottomCamera", root);
    world.entity(bottom_camera_entity)->transform.position = Vec3{0.0f, 0.0f, 3.2f};
    Camera* bottom_camera = world.add_component<Camera>(bottom_camera_entity);
    bottom_camera->screen = ScreenTarget::Bottom;
    bottom_camera->field_of_view = 70.0f;
    bottom_camera->cull_mask = 2;

    const EntityId top_triangle = world.create_entity("TopTriangle", root);
    MeshRenderer* top_renderer = world.add_component<MeshRenderer>(top_triangle);
    top_renderer->mesh_handle = top_mesh;
    top_renderer->material_handle = top_material;
    top_renderer->layer = 0;

    const EntityId bottom_triangle = world.create_entity("BottomTriangle", root);
    MeshRenderer* bottom_renderer = world.add_component<MeshRenderer>(bottom_triangle);
    bottom_renderer->mesh_handle = bottom_mesh;
    bottom_renderer->material_handle = bottom_material;
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

        world.entity(top_triangle)->transform.rotation = Vec3{0.0f, top_angle, 0.0f};
        world.entity(bottom_triangle)->transform.rotation =
            Vec3{bottom_angle * 0.35f, bottom_angle, bottom_angle * 0.2f};

        world.update_transforms();
        build_render_list(world, list);
        device->execute(list);

        ++frames;

        if (!report_written && frames == 120) {
            const u64 elapsed = osGetTime() - start_ms;
            char report[512];
            std::snprintf(report, sizeof(report),
                          "frames=%u\n"
                          "elapsed_ms=%llu\n"
                          "us_per_frame=%llu\n"
                          "draws=%u\n"
                          "cameras=%u\n",
                          static_cast<unsigned>(frames),
                          static_cast<unsigned long long>(elapsed),
                          static_cast<unsigned long long>((elapsed * 1000ull) / frames),
                          static_cast<unsigned>(list.draws().size()),
                          static_cast<unsigned>(list.cameras().size()));
            write_report(report);
            report_written = true;
        }
    }

    const u64 total_ms = osGetTime() - start_ms;
    char report[512];
    std::snprintf(report, sizeof(report),
                  "frames=%u\n"
                  "elapsed_ms=%llu\n"
                  "us_per_frame=%llu\n"
                  "draws=%u\n"
                  "cameras=%u\n",
                  static_cast<unsigned>(frames), static_cast<unsigned long long>(total_ms),
                  static_cast<unsigned long long>(frames > 0 ? (total_ms * 1000ull) / frames : 0),
                  static_cast<unsigned>(list.draws().size()),
                  static_cast<unsigned>(list.cameras().size()));
    write_report(report);

    device->shutdown();
    return 0;
}
