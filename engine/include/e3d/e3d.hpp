#pragma once

/// Public engine API. Project scripts should include only this header (or the
/// individual headers under e3d/) and never platform SDK headers.

#include "e3d/core/arena.hpp"
#include "e3d/core/guid.hpp"
#include "e3d/core/json.hpp"
#include "e3d/core/log.hpp"
#include "e3d/core/math.hpp"
#include "e3d/core/types.hpp"
#include "e3d/reflect/registry.hpp"
#include "e3d/render/render_list.hpp"
#include "e3d/render/renderer.hpp"
#include "e3d/render/types.hpp"
#include "e3d/scene/components.hpp"
#include "e3d/scene/scene_io.hpp"
#include "e3d/scene/world.hpp"
#include "e3d/script/script_component.hpp"
