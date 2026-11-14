#pragma once

#include "e3d/core/types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace e3d {

class World;

struct SceneLoadResult final {
    bool ok = false;
    EntityId root = kInvalidEntity;
    std::string error;
    std::vector<std::string> warnings;
};

/// Loads a scene document into the world, replacing its current contents.
SceneLoadResult load_scene(World& world, std::string_view text);

/// Serializes the world's single root entity. Returns an empty string on error.
std::string save_scene(const World& world, std::string& error);

/// Serializes one entity subtree, used by the editor to create prefabs.
std::string save_entity_json(const World& world, EntityId entity);

/// Instantiates a prefab node (or a scene document with a root) under `parent`.
/// GUIDs are regenerated and internal entity references remapped.
EntityId instantiate_prefab(World& world, std::string_view text, EntityId parent,
                            std::string& error);

} // namespace e3d
