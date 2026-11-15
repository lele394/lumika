#pragma once

#include "e3d/render/render_device.hpp"

#include <memory>

namespace e3d {

/// OpenGL 3.3 backend for the desktop editor. The GL context must be current.
std::unique_ptr<RenderDevice> create_render_device_gl();

} // namespace e3d
