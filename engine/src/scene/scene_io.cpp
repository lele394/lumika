#include "e3d/scene/scene_io.hpp"

#include "e3d/core/guid.hpp"
#include "e3d/core/json.hpp"
#include "e3d/reflect/registry.hpp"
#include "e3d/scene/components.hpp"
#include "e3d/scene/prop_io.hpp"
#include "e3d/scene/world.hpp"

#include <cstdio>
#include <string>
#include <utility>

namespace {

using namespace e3d;

const PropDesc* find_prop(const ComponentDesc& descriptor, std::string_view name) {
    for (u32 i = 0; i < descriptor.prop_count; ++i) {
        if (name == descriptor.props[i].name) return &descriptor.props[i];
    }
    return nullptr;
}

class SceneReader final {
public:
    SceneReader(World& world, SceneLoadResult& result) : world_(world), result_(result) {}

    void read_props(Component& component, const json::Value& props, const std::string& path,
                    bool skip_type_key) {
        for (std::size_t i = 0; i < props.size(); ++i) {
            const std::string& key = props.key_at(i);
            if (skip_type_key && key == "type") continue;

            const PropDesc* prop = find_prop(*component.descriptor(), key);
            if (prop == nullptr) {
                component.extra_props.set(key, props.value_at(i));
                continue;
            }

            std::string error;
            if (!read_property(component, *prop, props.value_at(i), error)) {
                fail(path + "." + key, error);
                return;
            }
        }
    }

    bool has_error() const noexcept { return !result_.error.empty(); }

    EntityId read_node(const json::Value& node, EntityId parent, const std::string& path) {
        if (!node.is_object()) {
            fail(path, "expected an object");
            return kInvalidEntity;
        }

        Guid guid;
        if (const json::Value* raw_guid = node.find("guid")) {
            if (!raw_guid->is_string() || !guid_from_string(raw_guid->as_string(), guid)) {
                fail(path + ".guid", "invalid guid");
                return kInvalidEntity;
            }
        } else {
            guid = generate_guid();
        }

        if (world_.find_by_guid(guid) != kInvalidEntity) {
            fail(path + ".guid", "duplicate entity guid " + guid_to_string(guid));
            return kInvalidEntity;
        }

        std::string name;
        if (const json::Value* raw_name = node.find("name")) {
            if (!raw_name->is_string()) {
                fail(path + ".name", "expected a string");
                return kInvalidEntity;
            }
            name = raw_name->as_string();
        }

        const EntityId id = world_.create_entity_with_guid(guid, name, parent);
        Entity* entity = world_.entity(id);
        if (entity == nullptr) {
            fail(path, "failed to create entity");
            return kInvalidEntity;
        }

        if (const json::Value* transform = node.find("transform")) {
            if (!read_transform(*transform, entity->transform, path + ".transform")) {
                return kInvalidEntity;
            }
        }

        if (const json::Value* components = node.find("components")) {
            if (!components->is_array()) {
                fail(path + ".components", "expected an array");
                return kInvalidEntity;
            }

            for (std::size_t i = 0; i < components->size(); ++i) {
                const std::string component_path =
                    path + ".components[" + std::to_string(i) + "]";
                const json::Value& component_json = (*components)[i];

                if (!component_json.is_object()) {
                    fail(component_path, "expected an object");
                    return kInvalidEntity;
                }

                const json::Value* type = component_json.find("type");
                if (type == nullptr || !type->is_string()) {
                    fail(component_path, "missing component type");
                    return kInvalidEntity;
                }

                const ComponentDesc* descriptor = find_component_desc(type->as_string());
                if (descriptor == nullptr) {
                    world_.unresolved().push_back({id, component_json});
                    result_.warnings.push_back(component_path + ": unknown component type '" +
                                               type->as_string() + "' preserved as-is");
                    continue;
                }

                Component* component = world_.add_component(id, *descriptor);
                if (component == nullptr) {
                    fail(component_path, "failed to create component");
                    return kInvalidEntity;
                }

                if (const json::Value* props = component_json.find("props")) {
                    if (!props->is_object()) {
                        fail(component_path + ".props", "expected an object");
                        return kInvalidEntity;
                    }
                    read_props(*component, *props, component_path + ".props", false);
                } else {
                    // Accept flattened properties for compatibility with the
                    // preliminary format in prelim_design.md.
                    read_props(*component, component_json, component_path, true);
                }

                if (has_error()) return kInvalidEntity;
            }
        }

        if (const json::Value* children = node.find("children")) {
            if (!children->is_array()) {
                fail(path + ".children", "expected an array");
                return kInvalidEntity;
            }
            for (std::size_t i = 0; i < children->size(); ++i) {
                read_node((*children)[i], id, path + ".children[" + std::to_string(i) + "]");
                if (has_error()) return kInvalidEntity;
            }
        }

        return id;
    }

private:
    void fail(const std::string& path, const std::string& message) {
        if (result_.error.empty()) result_.error = path + ": " + message;
    }

    bool read_vec3(const json::Value& parent, const char* key, Vec3& out, const std::string& path) {
        const json::Value* value = parent.find(key);
        if (value == nullptr) return true;

        if (!value->is_array() || value->size() != 3) {
            fail(path + "." + key, "expected [x, y, z]");
            return false;
        }
        for (std::size_t i = 0; i < 3; ++i) {
            if (!(*value)[i].is_number()) {
                fail(path + "." + key, "expected [x, y, z] numbers");
                return false;
            }
        }

        out = Vec3{static_cast<float>((*value)[0].as_number()),
                   static_cast<float>((*value)[1].as_number()),
                   static_cast<float>((*value)[2].as_number())};
        return true;
    }

    bool read_transform(const json::Value& transform, Transform& out, const std::string& path) {
        if (!transform.is_object()) {
            fail(path, "expected an object");
            return false;
        }
        if (!read_vec3(transform, "position", out.position, path)) return false;
        if (!read_vec3(transform, "rotation", out.rotation, path)) return false;
        if (!read_vec3(transform, "scale", out.scale, path)) return false;
        return true;
    }

    World& world_;
    SceneLoadResult& result_;
};

void resolve_entity_references(World& world, std::vector<std::string>& warnings) {
    for (const auto& slot : world.entities()) {
        if (slot == nullptr) continue;

        for (Component* component : slot->components) {
            const ComponentDesc* descriptor = component->descriptor();
            if (descriptor == nullptr) continue;

            for (u32 i = 0; i < descriptor->prop_count; ++i) {
                const PropDesc& prop = descriptor->props[i];
                if (prop.type != PropType::EntityRef) continue;

                auto* reference = static_cast<EntityRef*>(prop.get(component));
                if (reference == nullptr || !reference->guid.valid()) continue;

                const EntityId target = world.find_by_guid(reference->guid);
                if (target != kInvalidEntity) {
                    reference->target = target;
                } else {
                    warnings.push_back("entity '" + slot->name + "' component '" +
                                       descriptor->name + "' property '" + prop.name +
                                       "': unresolved reference " +
                                       guid_to_string(reference->guid));
                }
            }
        }
    }
}

json::Value write_transform(const Transform& transform) {
    const auto write_vec3 = [](Vec3 value) {
        json::Value array = json::Value::array();
        array.push(json::Value::number(value.x));
        array.push(json::Value::number(value.y));
        array.push(json::Value::number(value.z));
        return array;
    };

    json::Value object = json::Value::object();
    object.set("position", write_vec3(transform.position));
    object.set("rotation", write_vec3(transform.rotation));
    object.set("scale", write_vec3(transform.scale));
    return object;
}

json::Value write_node(const World& world, const Entity& entity) {
    json::Value node = json::Value::object();
    node.set("guid", json::Value::string(guid_to_string(entity.guid)));
    node.set("name", json::Value::string(entity.name));
    node.set("transform", write_transform(entity.transform));

    json::Value components = json::Value::array();
    for (const Component* component : entity.components) {
        const ComponentDesc* descriptor = component->descriptor();
        if (descriptor == nullptr) continue;

        json::Value component_json = json::Value::object();
        component_json.set("type", json::Value::string(descriptor->name));

        json::Value props = json::Value::object();
        for (u32 i = 0; i < descriptor->prop_count; ++i) {
            const PropDesc& prop = descriptor->props[i];
            props.set(prop.name, write_property(*component, prop, &world));
        }

        if (component->extra_props.is_object()) {
            for (std::size_t i = 0; i < component->extra_props.size(); ++i) {
                const std::string& key = component->extra_props.key_at(i);
                if (props.find(key) == nullptr) {
                    props.set(key, component->extra_props.value_at(i));
                }
            }
        }

        component_json.set("props", props);
        components.push(component_json);
    }

    for (const World::UnresolvedComponent& unresolved : world.unresolved()) {
        if (unresolved.entity == entity.id) components.push(unresolved.value);
    }

    node.set("components", components);

    json::Value children = json::Value::array();
    for (EntityId child_id : entity.children) {
        if (const Entity* child = world.entity(child_id)) {
            children.push(write_node(world, *child));
        }
    }
    node.set("children", children);

    return node;
}

void regenerate_guids(json::Value& node, std::vector<std::pair<Guid, Guid>>& map) {
    if (!node.is_object()) return;

    if (json::Value* guid_value = node.find("guid"); guid_value != nullptr &&
                                                     guid_value->is_string()) {
        Guid old_guid;
        if (guid_from_string(guid_value->as_string(), old_guid)) {
            const Guid fresh = generate_guid();
            map.push_back({old_guid, fresh});
            *guid_value = json::Value::string(guid_to_string(fresh));
        }
    }

    if (json::Value* children = node.find("children"); children != nullptr &&
                                                      children->is_array()) {
        for (std::size_t i = 0; i < children->size(); ++i) {
            regenerate_guids((*children)[i], map);
        }
    }
}

void remap_entity_refs(json::Value& value, const std::vector<std::pair<Guid, Guid>>& map) {
    if (value.is_object()) {
        if (json::Value* ref = value.find("$entity"); ref != nullptr && ref->is_string()) {
            Guid old_guid;
            if (guid_from_string(ref->as_string(), old_guid)) {
                for (const auto& entry : map) {
                    if (entry.first == old_guid) {
                        *ref = json::Value::string(guid_to_string(entry.second));
                        break;
                    }
                }
            }
        }
        for (std::size_t i = 0; i < value.size(); ++i) {
            remap_entity_refs(value.value_at(i), map);
        }
    } else if (value.is_array()) {
        for (std::size_t i = 0; i < value.size(); ++i) {
            remap_entity_refs(value[i], map);
        }
    }
}

} // namespace

namespace e3d {

SceneLoadResult load_scene(World& world, std::string_view text) {
    SceneLoadResult result;

    register_builtin_components();

    json::Value document;
    json::ParseError parse_error;
    if (!json::parse(text, document, parse_error)) {
        result.error = "json parse error at offset " + std::to_string(parse_error.offset) + ": " +
                       parse_error.message;
        return result;
    }

    if (!document.is_object()) {
        result.error = "scene document must be an object";
        return result;
    }

    const json::Value* version = document.find("format_version");
    if (version == nullptr || !version->is_number() || version->as_int64() != 1) {
        result.error = "unsupported or missing format_version (expected 1)";
        return result;
    }

    const json::Value* root = document.find("root");
    if (root == nullptr || !root->is_object()) {
        result.error = "missing root object";
        return result;
    }

    world.clear();

    if (const json::Value* name = document.find("name"); name != nullptr && name->is_string()) {
        world.set_name(name->as_string());
    }

    SceneReader reader(world, result);
    result.root = reader.read_node(*root, kInvalidEntity, "root");

    if (!result.ok && !result.error.empty()) {
        world.clear();
        return result;
    }

    resolve_entity_references(world, result.warnings);

    result.ok = result.error.empty();
    if (!result.ok) world.clear();
    return result;
}

std::string save_scene(const World& world, std::string& error) {
    error.clear();

    const Entity* root = nullptr;
    int root_count = 0;
    for (const auto& slot : world.entities()) {
        if (slot != nullptr && slot->parent == kInvalidEntity) {
            root = slot.get();
            ++root_count;
        }
    }

    if (root == nullptr || root_count != 1) {
        error = "scene must have exactly one root entity (found " + std::to_string(root_count) + ")";
        return {};
    }

    json::Value document = json::Value::object();
    document.set("format_version", json::Value::number(1));
    document.set("name", json::Value::string(world.name()));
    document.set("root", write_node(world, *root));

    return json::dump(document, true, 2);
}

std::string save_entity_json(const World& world, EntityId entity_id) {
    const Entity* entity = world.entity(entity_id);
    if (entity == nullptr) return {};
    return json::dump(write_node(world, *entity), true, 2);
}

EntityId instantiate_prefab(World& world, std::string_view text, EntityId parent,
                            std::string& error) {
    error.clear();

    json::Value document;
    json::ParseError parse_error;
    if (!json::parse(text, document, parse_error)) {
        error = "prefab: " + parse_error.message;
        return kInvalidEntity;
    }

    if (!document.is_object()) {
        error = "prefab: expected an object";
        return kInvalidEntity;
    }

    const json::Value* root = document.find("root");
    json::Value node = root != nullptr ? *root : document;
    if (!node.is_object()) {
        error = "prefab: invalid root node";
        return kInvalidEntity;
    }

    std::vector<std::pair<Guid, Guid>> remap;
    regenerate_guids(node, remap);
    remap_entity_refs(node, remap);

    SceneLoadResult result;
    SceneReader reader(world, result);
    const EntityId entity = reader.read_node(node, parent, "prefab");
    if (!result.error.empty()) {
        error = result.error;
        return kInvalidEntity;
    }

    std::vector<std::string> warnings;
    resolve_entity_references(world, warnings);
    world.update_transforms();
    return entity;
}

} // namespace e3d
