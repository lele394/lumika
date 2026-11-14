#pragma once

#include "e3d/core/json.hpp"
#include "e3d/core/types.hpp"
#include "e3d/scene/entity.hpp"
#include "e3d/reflect/registry.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace e3d {

class World final {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void set_name(std::string_view name) { name_.assign(name); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    EntityId create_entity(std::string_view name, EntityId parent = kInvalidEntity);
    EntityId create_entity_with_guid(const Guid& guid, std::string_view name,
                                     EntityId parent = kInvalidEntity);
    void destroy_entity(EntityId id);
    void clear();

    [[nodiscard]] bool valid(EntityId id) const noexcept;
    [[nodiscard]] Entity* entity(EntityId id) noexcept;
    [[nodiscard]] const Entity* entity(EntityId id) const noexcept;

    [[nodiscard]] EntityId find_by_guid(const Guid& guid) const noexcept;
    [[nodiscard]] Guid guid_of(EntityId id) const noexcept;

    void set_parent(EntityId child, EntityId parent);

    Component* add_component(EntityId id, const ComponentDesc& descriptor);
    Component* add_component(EntityId id, std::string_view type_name);
    [[nodiscard]] Component* find_component(EntityId id, TypeId type) const noexcept;
    void remove_component(Component* component);

    template <typename T>
    T* add_component(EntityId id) {
        return static_cast<T*>(add_component(id, *T::static_descriptor()));
    }

    template <typename T>
    [[nodiscard]] T* find_component(EntityId id) const noexcept {
        return static_cast<T*>(find_component(id, T::static_type_id()));
    }

    void update_transforms();

    [[nodiscard]] EntityId root() const noexcept;

    [[nodiscard]] const std::vector<std::unique_ptr<Entity>>& entities() const noexcept {
        return slots_;
    }

    /// Components whose type was not registered when the scene was loaded. They are
    /// written back unchanged on save so nothing is lost.
    struct UnresolvedComponent final {
        EntityId entity = kInvalidEntity;
        json::Value value;
    };

    [[nodiscard]] std::vector<UnresolvedComponent>& unresolved() noexcept { return unresolved_; }
    [[nodiscard]] const std::vector<UnresolvedComponent>& unresolved() const noexcept {
        return unresolved_;
    }

private:
    EntityId allocate_slot();
    void destroy_components(Entity& entity);
    void update_recursive(Entity& entity, const Mat4& parent_world);

    std::vector<std::unique_ptr<Entity>> slots_;
    std::vector<u8> generations_;
    std::vector<u32> free_slots_;
    std::vector<UnresolvedComponent> unresolved_;
    std::string name_;
};

} // namespace e3d
