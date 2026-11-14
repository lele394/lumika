#pragma once

#include "e3d/assets/file_system.hpp"

namespace e3d {

/// FileSystem rooted at the RomFS embedded in the .3dsx (`romfs:/`).
class RomFsFileSystem final : public FileSystem {
public:
    bool read(std::string_view path, std::vector<u8>& out) override;
};

} // namespace e3d
