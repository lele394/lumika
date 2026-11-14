#include "e3d/render/renderer.hpp"

#include "e3d/scene/components.hpp"
#include "e3d/scene/world.hpp"

namespace e3d {

namespace {

struct CameraEntry final {
    u8 index = 0;
    u32 mask = 0;
};

} // namespace

void build_render_list(const World& world, RenderList& list) {
    list.clear();

    const Environment* environment = nullptr;
    for (const auto& slot : world.entities()) {
        if (slot == nullptr) continue;
        if (const Environment* found = slot->find_component<Environment>(); found != nullptr) {
            environment = found;
            break;
        }
    }

    const Color clear = environment != nullptr ? environment->clear_color
                                               : Color{0.04f, 0.05f, 0.08f, 1.0f};

    CameraEntry entries[16];
    int camera_count = 0;

    for (const auto& slot : world.entities()) {
        if (slot == nullptr) continue;
        const Camera* camera = slot->find_component<Camera>();
        if (camera == nullptr) continue;

        CameraDesc desc;
        desc.screen = camera->screen == ScreenTarget::Top ? Screen::Top : Screen::Bottom;
        desc.field_of_view = camera->field_of_view;
        desc.near_plane = camera->near_plane;
        desc.far_plane = camera->far_plane;
        desc.stereo = camera->stereo;
        desc.cull_mask = static_cast<u32>(camera->cull_mask);
        desc.clear = clear;

        const Mat4& world_matrix = slot->world;
        desc.position = Vec3{world_matrix.m[12], world_matrix.m[13], world_matrix.m[14]};

        const Vec3 up{world_matrix.m[4], world_matrix.m[5], world_matrix.m[6]};
        const Vec3 back{world_matrix.m[8], world_matrix.m[9], world_matrix.m[10]};

        desc.up = normalize(up);
        desc.target = desc.position - normalize(back);

        const u8 index = list.add_camera(desc);
        if (camera_count < 16) {
            entries[camera_count].index = index;
            entries[camera_count].mask = desc.cull_mask;
            ++camera_count;
        }
    }

    for (const auto& slot : world.entities()) {
        if (slot == nullptr) continue;
        const MeshRenderer* renderer = slot->find_component<MeshRenderer>();
        if (renderer == nullptr) continue;
        if (!renderer->mesh_handle.valid() || !renderer->material_handle.valid()) continue;

        const u32 layer = static_cast<u32>(renderer->layer) & 31u;
        const u32 bit = 1u << layer;

        for (int i = 0; i < camera_count; ++i) {
            if ((entries[i].mask & bit) == 0) continue;
            list.draw(entries[i].index, renderer->mesh_handle, renderer->material_handle,
                      slot->world);
        }
    }
}

} // namespace e3d
