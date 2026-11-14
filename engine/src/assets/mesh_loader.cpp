#include "e3d/assets/mesh_loader.hpp"

#include <cstdint>
#include <cstring>

namespace e3d {

namespace {

constexpr u32 kCmeshVersion = 1;

struct CmeshHeader final {
    char magic[4];
    u32 version;
    u32 vertex_count;
    u32 index_count;
    u32 vertex_stride;
    u32 flags;
    float aabb_min[3];
    float aabb_max[3];
    u32 reserved[2];
};

static_assert(sizeof(CmeshHeader) == 56, "cmesh header layout changed");

} // namespace

MeshData load_cmesh(const void* data, std::size_t size, std::string& error) {
    error.clear();

    if (data == nullptr || size < sizeof(CmeshHeader)) {
        error = "cmesh: truncated header";
        return {};
    }

    CmeshHeader header;
    std::memcpy(&header, data, sizeof(header));

    if (std::memcmp(header.magic, "E3DM", 4) != 0) {
        error = "cmesh: bad magic";
        return {};
    }
    if (header.version != kCmeshVersion) {
        error = "cmesh: unsupported version " + std::to_string(header.version);
        return {};
    }
    if (header.vertex_stride != sizeof(MeshVertex)) {
        error = "cmesh: vertex stride mismatch";
        return {};
    }
    if (header.vertex_count == 0 || header.index_count == 0) {
        error = "cmesh: empty mesh";
        return {};
    }

    const std::size_t vertex_bytes = static_cast<std::size_t>(header.vertex_count) * sizeof(MeshVertex);
    const std::size_t index_bytes = static_cast<std::size_t>(header.index_count) * sizeof(u16);
    const std::size_t expected = sizeof(CmeshHeader) + vertex_bytes + index_bytes;
    if (size < expected) {
        error = "cmesh: truncated payload";
        return {};
    }

    const auto* vertex_data = reinterpret_cast<const unsigned char*>(data) + sizeof(CmeshHeader);
    const auto* index_data = vertex_data + vertex_bytes;

    MeshData mesh;
    mesh.vertices.resize(header.vertex_count);
    mesh.indices.resize(header.index_count);
    std::memcpy(mesh.vertices.data(), vertex_data, vertex_bytes);
    std::memcpy(mesh.indices.data(), index_data, index_bytes);

    for (u16 index : mesh.indices) {
        if (index >= header.vertex_count) {
            error = "cmesh: index out of range";
            return {};
        }
    }

    return mesh;
}

} // namespace e3d
