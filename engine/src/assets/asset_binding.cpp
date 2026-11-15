#include "e3d/assets/asset_binding.hpp"

#include "e3d/assets/asset_manager.hpp"
#include "e3d/core/guid.hpp"
#include "e3d/scene/components.hpp"
#include "e3d/scene/world.hpp"

namespace e3d {

void resolve_scene_assets(World& world, AssetManager& assets,
                          std::vector<std::string>& warnings) {
    for (const auto& slot : world.entities()) {
        if (slot == nullptr) continue;

        std::string error;

        if (MeshRenderer* renderer = slot->find_component<MeshRenderer>(); renderer != nullptr) {
            if (renderer->mesh.guid.valid()) {
                renderer->mesh_handle = assets.mesh(renderer->mesh.guid, error);
                if (!renderer->mesh_handle.valid()) {
                    warnings.push_back("entity '" + slot->name + "': " + error);
                }
            }
            error.clear();
            if (renderer->material.guid.valid()) {
                renderer->material_handle = assets.material(renderer->material.guid, error);
                if (!renderer->material_handle.valid()) {
                    warnings.push_back("entity '" + slot->name + "': " + error);
                }
            }
        }

        error.clear();
        if (AudioSource* source = slot->find_component<AudioSource>(); source != nullptr) {
            if (source->clip.guid.valid()) {
                source->clip_handle = assets.audio_clip(source->clip.guid, error);
                if (!source->clip_handle.valid()) {
                    warnings.push_back("entity '" + slot->name + "': " + error);
                }
            }
        }

        error.clear();
        if (UiCanvas* canvas = slot->find_component<UiCanvas>(); canvas != nullptr) {
            if (canvas->font.guid.valid()) {
                canvas->font_texture = assets.texture(canvas->font.guid, error);
                if (!canvas->font_texture.valid()) {
                    warnings.push_back("entity '" + slot->name + "': " + error);
                }
            }
        }

        error.clear();
        if (UiImage* image = slot->find_component<UiImage>(); image != nullptr) {
            if (image->texture.guid.valid()) {
                image->texture_handle = assets.texture(image->texture.guid, error);
                if (!image->texture_handle.valid()) {
                    warnings.push_back("entity '" + slot->name + "': " + error);
                }
            }
        }
    }
}

} // namespace e3d
