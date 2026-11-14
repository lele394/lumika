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

} // namespace e3d
