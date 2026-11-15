#pragma once

#include "e3d/core/types.hpp"

namespace e3d {

class World;
class RenderList;
class UiCanvas;
class Entity;

/// Finds the nearest ancestor (excluding the element itself) that owns a
/// UiCanvas. UI elements inherit its screen and font.
const UiCanvas* ui_canvas_of(const World& world, const Entity& element);

/// Appends sprites for every visible UiText/UiImage below a UiCanvas.
void build_ui_render_list(const World& world, RenderList& list);

} // namespace e3d
