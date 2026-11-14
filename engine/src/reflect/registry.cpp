#include "e3d/reflect/registry.hpp"

namespace {

std::vector<const e3d::ComponentDesc*>& storage() {
    static std::vector<const e3d::ComponentDesc*> descriptors;
    return descriptors;
}

} // namespace

namespace e3d {

bool register_component(const ComponentDesc* descriptor) {
    if (descriptor == nullptr || descriptor->create == nullptr || descriptor->destroy == nullptr) {
        return false;
    }

    auto& all = storage();
    for (const ComponentDesc* existing : all) {
        if (existing->id == descriptor->id) {
            return true;  // registration is idempotent
        }
    }
    all.push_back(descriptor);
    return true;
}

const ComponentDesc* find_component_desc(std::string_view name) {
    return find_component_desc(hash_name(name));
}

const ComponentDesc* find_component_desc(TypeId id) {
    for (const ComponentDesc* descriptor : storage()) {
        if (descriptor->id == id) return descriptor;
    }
    return nullptr;
}

const std::vector<const ComponentDesc*>& all_components() { return storage(); }

} // namespace e3d
