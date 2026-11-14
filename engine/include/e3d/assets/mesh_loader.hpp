#pragma once

#include "e3d/render/types.hpp"

#include <cstddef>
#include <string>

namespace e3d {

/// Parses a cooked `.cmesh` buffer into a MeshData. Returns an empty mesh and sets
/// `error` when the header or sizes are invalid.
MeshData load_cmesh(const void* data, std::size_t size, std::string& error);

} // namespace e3d
