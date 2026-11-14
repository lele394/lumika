#include "e3d/platform/3ds/file_system_3ds.hpp"

#include <cstdio>
#include <string>

namespace e3d {

bool RomFsFileSystem::read(std::string_view path, std::vector<u8>& out) {
    std::string full = "romfs:/";
    full.append(path);

    FILE* file = std::fopen(full.c_str(), "rb");
    if (file == nullptr) return false;

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(file);
        return false;
    }

    out.resize(static_cast<std::size_t>(size));
    const std::size_t read = std::fread(out.data(), 1, out.size(), file);
    std::fclose(file);
    return read == out.size();
}

} // namespace e3d
