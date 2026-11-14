#pragma once

#include "e3d/core/types.hpp"

#include <string_view>
#include <vector>

namespace e3d {

/// Read-only file access, rooted at an implementation-defined location
/// (RomFS on device, a project directory on the host).
class FileSystem {
public:
    virtual ~FileSystem() = default;
    virtual bool read(std::string_view path, std::vector<u8>& out) = 0;
};

} // namespace e3d
