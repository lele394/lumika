#include "e3d/assets/directory_file_system.hpp"

#include <fstream>

namespace e3d {

DirectoryFileSystem::DirectoryFileSystem(std::string root) : root_(std::move(root)) {
    while (!root_.empty() && (root_.back() == '/' || root_.back() == '\\')) root_.pop_back();
}

bool DirectoryFileSystem::read(std::string_view path, std::vector<u8>& out) {
    std::string full = root_;
    full.push_back('/');
    full.append(path);

    std::ifstream file(full, std::ios::binary);
    if (!file) return false;

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0) return false;

    out.resize(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(out.data()), size);
    return file.good() || file.eof();
}

} // namespace e3d
