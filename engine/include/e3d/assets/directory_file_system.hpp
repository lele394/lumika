#pragma once

#include "e3d/assets/file_system.hpp"

#include <string>

namespace e3d {

/// Host-side FileSystem rooted at a directory. Also works for `sdmc:/` paths.
class DirectoryFileSystem final : public FileSystem {
public:
    explicit DirectoryFileSystem(std::string root);

    bool read(std::string_view path, std::vector<u8>& out) override;

private:
    std::string root_;
};

} // namespace e3d
