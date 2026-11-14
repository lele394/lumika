#pragma once

#include "e3d/render/render_list.hpp"

namespace e3d {

class World;

/// Walks the world, collects cameras and mesh renderers, and fills the list.
/// Filtering is by camera `cull_mask` bit versus renderer `layer`.
void build_render_list(const World& world, RenderList& list);

} // namespace e3d
