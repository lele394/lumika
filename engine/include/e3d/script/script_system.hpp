#pragma once

#include "e3d/core/types.hpp"

namespace e3d {

class World;

/// Deterministic lifecycle dispatch. Scripts run in entity creation order and
/// component attachment order.
void initialize_scripts(World& world);
void update_scripts(World& world, float delta_time);
void fixed_update_scripts(World& world, float fixed_delta_time);
void shutdown_scripts(World& world);

} // namespace e3d
