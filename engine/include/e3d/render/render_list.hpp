#pragma once

#include "e3d/core/math.hpp"
#include "e3d/core/types.hpp"
#include "e3d/render/types.hpp"

#include <vector>

namespace e3d {

/// Per-frame camera description. Projection is built by the backend so each one
/// can apply its native clip-space conventions.
struct CameraDesc final {
    Screen screen = Screen::Top;
    Vec3 position{0.0f, 0.0f, 4.0f};
    Vec3 target{0.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    float field_of_view = 50.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;
    Color clear{0.04f, 0.05f, 0.08f, 1.0f};
    bool clear_depth = true;
    bool stereo = false;
    u32 cull_mask = 0xFFFFFFFFu;
};

struct DrawItem final {
    u8 camera = 0;
    MeshHandle mesh;
    MaterialHandle material;
    Mat4 model = mat4_identity();
};

/// Built once per frame by the renderer and consumed by a backend. Capacity is
/// retained between frames so the steady state performs no allocations.
class RenderList final {
public:
    RenderList();

    void clear();
    u8 add_camera(const CameraDesc& camera);
    void draw(u8 camera, MeshHandle mesh, MaterialHandle material, const Mat4& model);
    void draw_sprite(const SpriteItem& sprite);

    [[nodiscard]] const std::vector<CameraDesc>& cameras() const noexcept { return cameras_; }
    [[nodiscard]] const std::vector<DrawItem>& draws() const noexcept { return draws_; }
    [[nodiscard]] const std::vector<SpriteItem>& sprites() const noexcept { return sprites_; }

private:
    std::vector<CameraDesc> cameras_;
    std::vector<DrawItem> draws_;
    std::vector<SpriteItem> sprites_;
};

} // namespace e3d
