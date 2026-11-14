#pragma once

#include "e3d/core/math.hpp"
#include "e3d/core/types.hpp"
#include "e3d/reflect/registry.hpp"
#include "e3d/scene/component.hpp"

#include <string>
#include <vector>

namespace e3d {

/// Local transform. Rotation is Euler degrees (X then Y then Z) because that is
/// what is authored in JSON and it keeps save/load lossless.
struct Transform final {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 rotation{0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    [[nodiscard]] Mat4 local_matrix() const noexcept {
        return mat4_translation(position) * mat4_from_quat(quat_from_euler_xyz_degrees(rotation)) *
               mat4_scale(scale);
    }
};

struct Entity final {
    Guid guid;
    EntityId id = kInvalidEntity;
    EntityId parent = kInvalidEntity;
    std::string name;
    Transform transform;
    Mat4 world = mat4_identity();

    std::vector<EntityId> children;
    std::vector<Component*> components;

    [[nodiscard]] Component* find_component(TypeId type) const {
        for (Component* component : components) {
            if (component->descriptor()->id == type) return component;
        }
        return nullptr;
    }

    template <typename T>
    [[nodiscard]] T* find_component() const {
        return static_cast<T*>(find_component(T::static_type_id()));
    }
};

} // namespace e3d
