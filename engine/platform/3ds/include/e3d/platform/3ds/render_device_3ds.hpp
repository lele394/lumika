#pragma once

#include "e3d/render/render_device.hpp"

#include <memory>

namespace e3d {

/// Creates the Citro3D backend. The device owns gfx/C3D initialization, so
/// gfxInitDefault() must not be called separately when using it.
std::unique_ptr<RenderDevice> create_render_device_3ds();

} // namespace e3d
