#pragma once

#include "e3d/core/json.hpp"
#include "e3d/reflect/registry.hpp"
#include "e3d/scene/component.hpp"

#include <string>

namespace e3d {

class World;

/// Reads one property value from JSON into the component member. On failure the
/// member is left untouched and `error` explains what was expected.
bool read_property(Component& component, const PropDesc& prop, const json::Value& value,
                   std::string& error);

/// Serializes one property. `world` is used to recover GUIDs for entity refs that
/// were set at runtime rather than loaded from data.
json::Value write_property(const Component& component, const PropDesc& prop, const World* world);

} // namespace e3d
