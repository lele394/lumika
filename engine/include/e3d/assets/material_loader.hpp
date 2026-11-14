#pragma once

#include "e3d/render/types.hpp"

#include <string>
#include <string_view>

namespace e3d {

/// Parses a `.mat.json` file. `texture_guid` is valid when the material has a
/// texture slot; the caller resolves it to a TextureHandle.
bool parse_material(std::string_view text, MaterialDesc& material, Guid& texture_guid,
                    std::string& error);

} // namespace e3d
