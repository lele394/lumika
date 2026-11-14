#pragma once

#include <e3d/e3d.hpp>

namespace demo {

/// Appends a coloured cube (24 vertices, 36 indices) to a mesh.
inline void append_cube(e3d::MeshData& mesh, e3d::Vec3 center, float size, e3d::Color color,
                        const e3d::Mat4& transform = e3d::mat4_identity()) {
    const float h = size * 0.5f;

    struct Face final {
        e3d::Vec3 normal;
        e3d::Vec3 corners[4];
    };

    const Face faces[6] = {
        {{0, 0, 1}, {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}}},
        {{0, 0, -1}, {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}}},
        {{1, 0, 0}, {{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}}},
        {{-1, 0, 0}, {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}}},
        {{0, 1, 0}, {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}}},
        {{0, -1, 0}, {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}}},
    };

    for (const Face& face : faces) {
        const e3d::u16 base = static_cast<e3d::u16>(mesh.vertices.size());
        const e3d::Vec3 normal = e3d::normalize(e3d::transform_direction(transform, face.normal));

        for (int i = 0; i < 4; ++i) {
            e3d::MeshVertex vertex;
            vertex.position = e3d::transform_point(transform, face.corners[i] + center);
            vertex.color = color;
            vertex.uv = e3d::Vec2{0.0f, 0.0f};
            vertex.normal = normal;
            mesh.vertices.push_back(vertex);
        }

        const e3d::u16 indices[6] = {base, static_cast<e3d::u16>(base + 1),
                                     static_cast<e3d::u16>(base + 2), base,
                                     static_cast<e3d::u16>(base + 2),
                                     static_cast<e3d::u16>(base + 3)};
        for (e3d::u16 index : indices) mesh.indices.push_back(index);
    }
}

} // namespace demo
