#pragma once

#include "e3d/core/types.hpp"

#include <string>
#include <string_view>

namespace e3d {

/// Formats as a lowercase RFC 4122 style string: 8-4-4-4-12.
std::string guid_to_string(const Guid& guid);

/// Accepts 32 hex digits, with or without hyphens. Rejects any other shape.
bool guid_from_string(std::string_view text, Guid& out);

/// Random (non-cryptographic) v4-shaped guid, unique enough for project data.
Guid generate_guid();

} // namespace e3d
