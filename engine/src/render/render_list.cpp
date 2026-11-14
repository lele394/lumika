#include "e3d/render/render_list.hpp"

namespace e3d {

RenderList::RenderList() {
    cameras_.reserve(8);
    draws_.reserve(256);
    sprites_.reserve(1024);
}

void RenderList::clear() {
    cameras_.clear();
    draws_.clear();
    sprites_.clear();
}

u8 RenderList::add_camera(const CameraDesc& camera) {
    if (cameras_.size() >= 255) return 0;
    cameras_.push_back(camera);
    return static_cast<u8>(cameras_.size() - 1);
}

void RenderList::draw(u8 camera, MeshHandle mesh, MaterialHandle material, const Mat4& model) {
    if (camera >= cameras_.size() || !mesh.valid() || !material.valid()) return;

    DrawItem item;
    item.camera = camera;
    item.mesh = mesh;
    item.material = material;
    item.model = model;
    draws_.push_back(item);
}

void RenderList::draw_sprite(const SpriteItem& sprite) {
    if (!sprite.texture.valid()) return;
    sprites_.push_back(sprite);
}

} // namespace e3d
