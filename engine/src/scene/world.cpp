#include "e3d/scene/world.hpp"

#include "e3d/core/guid.hpp"

#include <algorithm>

namespace e3d {

World::World() = default;

World::~World() { clear(); }

EntityId World::allocate_slot() {
    u32 index = kInvalidIndex;

    if (!free_slots_.empty()) {
        index = free_slots_.back();
        free_slots_.pop_back();

        u8 generation = static_cast<u8>(generations_[index] + 1u);
        if (generation == 0) generation = 1;
        generations_[index] = generation;
    } else {
        index = static_cast<u32>(slots_.size());
        slots_.push_back(nullptr);
        generations_.push_back(1);
    }

    return make_entity_id(index, generations_[index]);
}

EntityId World::create_entity(std::string_view name, EntityId parent) {
    return create_entity_with_guid(generate_guid(), name, parent);
}

EntityId World::create_entity_with_guid(const Guid& guid, std::string_view name, EntityId parent) {
    if (parent != kInvalidEntity && !valid(parent)) return kInvalidEntity;

    const EntityId id = allocate_slot();
    const u32 index = entity_index(id);

    auto entity = std::make_unique<Entity>();
    entity->guid = guid;
    entity->id = id;
    entity->parent = parent;
    entity->name.assign(name);
    entity->world = mat4_identity();

    slots_[index] = std::move(entity);

    if (parent != kInvalidEntity) {
        Entity* parent_entity = this->entity(parent);
        if (parent_entity != nullptr) parent_entity->children.push_back(id);
    }

    return id;
}

void World::destroy_components(Entity& entity) {
    for (Component* component : entity.components) {
        if (component == nullptr) continue;
        const ComponentDesc* descriptor = component->descriptor();
        if (descriptor != nullptr && descriptor->destroy != nullptr) descriptor->destroy(component);
    }
    entity.components.clear();
}

void World::destroy_entity(EntityId id) {
    Entity* target = entity(id);
    if (target == nullptr) return;

    const std::vector<EntityId> children = target->children;
    for (EntityId child : children) destroy_entity(child);

    if (Entity* parent = entity(target->parent); parent != nullptr) {
        auto& siblings = parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
    }

    destroy_components(*target);

    unresolved_.erase(std::remove_if(unresolved_.begin(), unresolved_.end(),
                                     [id](const UnresolvedComponent& entry) {
                                         return entry.entity == id;
                                     }),
                      unresolved_.end());

    const u32 index = entity_index(id);
    if (index < slots_.size()) slots_[index].reset();
    free_slots_.push_back(index);
}

void World::clear() {
    for (auto& slot : slots_) {
        if (slot != nullptr) destroy_components(*slot);
    }
    slots_.clear();
    generations_.clear();
    free_slots_.clear();
    unresolved_.clear();
    name_.clear();
}

bool World::valid(EntityId id) const noexcept {
    if (id == kInvalidEntity) return false;
    const u32 index = entity_index(id);
    if (index >= slots_.size() || slots_[index] == nullptr) return false;
    return generations_[index] == entity_generation(id);
}

Entity* World::entity(EntityId id) noexcept {
    if (id == kInvalidEntity) return nullptr;
    const u32 index = entity_index(id);
    if (index >= slots_.size() || slots_[index] == nullptr) return nullptr;
    if (generations_[index] != entity_generation(id)) return nullptr;
    return slots_[index].get();
}

const Entity* World::entity(EntityId id) const noexcept {
    return const_cast<World*>(this)->entity(id);
}

EntityId World::find_by_guid(const Guid& guid) const noexcept {
    if (!guid.valid()) return kInvalidEntity;
    for (const auto& slot : slots_) {
        if (slot != nullptr && slot->guid == guid) return slot->id;
    }
    return kInvalidEntity;
}

Guid World::guid_of(EntityId id) const noexcept {
    const Entity* found = entity(id);
    return found != nullptr ? found->guid : Guid{};
}

void World::set_parent(EntityId child, EntityId parent) {
    Entity* child_entity = entity(child);
    if (child_entity == nullptr) return;
    if (child == parent) return;
    if (parent != kInvalidEntity && !valid(parent)) return;

    // Reject cycles: walking up from the new parent must never reach the child.
    for (EntityId cursor = parent; cursor != kInvalidEntity;) {
        if (cursor == child) return;
        const Entity* node = entity(cursor);
        if (node == nullptr) break;
        cursor = node->parent;
    }

    if (Entity* old_parent = entity(child_entity->parent); old_parent != nullptr) {
        auto& siblings = old_parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
    }

    child_entity->parent = parent;
    if (Entity* new_parent = entity(parent); new_parent != nullptr) {
        new_parent->children.push_back(child);
    }
}

Component* World::add_component(EntityId id, const ComponentDesc& descriptor) {
    Entity* owner = entity(id);
    if (owner == nullptr || descriptor.create == nullptr) return nullptr;

    Component* component = descriptor.create();
    if (component == nullptr) return nullptr;

    component->bind_entity(id);
    owner->components.push_back(component);
    return component;
}

Component* World::add_component(EntityId id, std::string_view type_name) {
    const ComponentDesc* descriptor = find_component_desc(type_name);
    if (descriptor == nullptr) return nullptr;
    return add_component(id, *descriptor);
}

Component* World::find_component(EntityId id, TypeId type) const noexcept {
    const Entity* owner = entity(id);
    return owner != nullptr ? owner->find_component(type) : nullptr;
}

void World::remove_component(Component* component) {
    if (component == nullptr) return;
    Entity* owner = entity(component->entity());
    if (owner == nullptr) return;

    auto& components = owner->components;
    const auto it = std::find(components.begin(), components.end(), component);
    if (it == components.end()) return;

    components.erase(it);
    const ComponentDesc* descriptor = component->descriptor();
    if (descriptor != nullptr && descriptor->destroy != nullptr) descriptor->destroy(component);
}

void World::update_recursive(Entity& entity, const Mat4& parent_world) {
    entity.world = parent_world * entity.transform.local_matrix();
    for (EntityId child : entity.children) {
        if (Entity* child_entity = this->entity(child); child_entity != nullptr) {
            update_recursive(*child_entity, entity.world);
        }
    }
}

void World::update_transforms() {
    const Mat4 identity = mat4_identity();
    for (auto& slot : slots_) {
        if (slot != nullptr && slot->parent == kInvalidEntity) {
            update_recursive(*slot, identity);
        }
    }
}

EntityId World::root() const noexcept {
    for (const auto& slot : slots_) {
        if (slot != nullptr && slot->parent == kInvalidEntity) return slot->id;
    }
    return kInvalidEntity;
}

} // namespace e3d
