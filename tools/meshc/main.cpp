// meshc: converts a Wavefront OBJ into the engine's `.cmesh` format.
// Usage: meshc <input.obj> <output.cmesh>

#include "e3d/core/math.hpp"
#include "e3d/render/types.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct OBJCorner final {
    int position = -1;
    int uv = -1;
    int normal = -1;
};

struct CmeshHeader final {
    char magic[4];
    e3d::u32 version;
    e3d::u32 vertex_count;
    e3d::u32 index_count;
    e3d::u32 vertex_stride;
    e3d::u32 flags;
    float aabb_min[3];
    float aabb_max[3];
    e3d::u32 reserved[2];
};

static_assert(sizeof(CmeshHeader) == 56, "cmesh header layout changed");

int resolve_index(int value, int count) {
    if (value > 0) return value - 1;
    if (value < 0) return count + value;
    return -1;
}

bool parse_corner(const std::string& token, OBJCorner& corner) {
    int values[3] = {0, 0, 0};
    int part = 0;
    std::string number;

    for (std::size_t i = 0; i <= token.size(); ++i) {
        const char c = i < token.size() ? token[i] : '/';
        if (c == '/') {
            if (!number.empty()) values[part] = std::atoi(number.c_str());
            number.clear();
            ++part;
            if (part >= 3) break;
        } else {
            number.push_back(c);
        }
    }

    corner.position = values[0];
    corner.uv = values[1];
    corner.normal = values[2];
    return corner.position != 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: meshc <input.obj> <output.cmesh>\n");
        return 1;
    }

    std::ifstream input(argv[1]);
    if (!input) {
        std::fprintf(stderr, "meshc: cannot open %s\n", argv[1]);
        return 1;
    }

    std::vector<e3d::Vec3> positions;
    std::vector<e3d::Vec2> uvs;
    std::vector<e3d::Vec3> normals;
    e3d::MeshData mesh;

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream stream(line);
        std::string tag;
        stream >> tag;

        if (tag == "v") {
            float x = 0, y = 0, z = 0;
            stream >> x >> y >> z;
            positions.push_back(e3d::Vec3{x, y, z});
        } else if (tag == "vt") {
            float u = 0, v = 0;
            stream >> u >> v;
            uvs.push_back(e3d::Vec2{u, v});
        } else if (tag == "vn") {
            float x = 0, y = 0, z = 0;
            stream >> x >> y >> z;
            normals.push_back(e3d::Vec3{x, y, z});
        } else if (tag == "f") {
            std::vector<OBJCorner> corners;
            std::string token;
            while (stream >> token) {
                OBJCorner corner;
                if (!parse_corner(token, corner)) continue;
                corner.position = resolve_index(corner.position, static_cast<int>(positions.size()));
                corner.uv = corner.uv != 0 ? resolve_index(corner.uv, static_cast<int>(uvs.size())) : -1;
                corner.normal =
                    corner.normal != 0 ? resolve_index(corner.normal, static_cast<int>(normals.size())) : -1;

                if (corner.position < 0 || corner.position >= static_cast<int>(positions.size())) {
                    std::fprintf(stderr, "meshc: bad vertex index in face\n");
                    return 1;
                }
                corners.push_back(corner);
            }

            if (corners.size() < 3) continue;

            for (std::size_t i = 1; i + 1 < corners.size(); ++i) {
                const OBJCorner* triangle[3] = {&corners[0], &corners[i], &corners[i + 1]};

                const e3d::Vec3 p0 = positions[static_cast<std::size_t>(triangle[0]->position)];
                const e3d::Vec3 p1 = positions[static_cast<std::size_t>(triangle[1]->position)];
                const e3d::Vec3 p2 = positions[static_cast<std::size_t>(triangle[2]->position)];
                e3d::Vec3 face_normal = e3d::normalize(e3d::cross(p1 - p0, p2 - p0));
                if (e3d::length(face_normal) < 0.5f) face_normal = e3d::Vec3{0.0f, 1.0f, 0.0f};

                for (const OBJCorner* corner : triangle) {
                    e3d::MeshVertex vertex;
                    vertex.position = positions[static_cast<std::size_t>(corner->position)];
                    vertex.color = e3d::Color{1.0f, 1.0f, 1.0f, 1.0f};

                    if (corner->uv >= 0 && corner->uv < static_cast<int>(uvs.size())) {
                        vertex.uv = uvs[static_cast<std::size_t>(corner->uv)];
                    }

                    if (corner->normal >= 0 && corner->normal < static_cast<int>(normals.size())) {
                        vertex.normal = e3d::normalize(
                            normals[static_cast<std::size_t>(corner->normal)]);
                    } else {
                        vertex.normal = face_normal;
                    }

                    if (mesh.vertices.size() >= 65535) {
                        std::fprintf(stderr, "meshc: too many vertices for 16-bit indices\n");
                        return 1;
                    }

                    mesh.indices.push_back(static_cast<e3d::u16>(mesh.vertices.size()));
                    mesh.vertices.push_back(vertex);
                }
            }
        }
    }

    if (mesh.vertices.empty() || mesh.indices.empty()) {
        std::fprintf(stderr, "meshc: no geometry in %s\n", argv[1]);
        return 1;
    }

    e3d::Vec3 aabb_min = mesh.vertices[0].position;
    e3d::Vec3 aabb_max = mesh.vertices[0].position;
    for (const e3d::MeshVertex& vertex : mesh.vertices) {
        aabb_min.x = std::min(aabb_min.x, vertex.position.x);
        aabb_min.y = std::min(aabb_min.y, vertex.position.y);
        aabb_min.z = std::min(aabb_min.z, vertex.position.z);
        aabb_max.x = std::max(aabb_max.x, vertex.position.x);
        aabb_max.y = std::max(aabb_max.y, vertex.position.y);
        aabb_max.z = std::max(aabb_max.z, vertex.position.z);
    }

    CmeshHeader header{};
    std::memcpy(header.magic, "E3DM", 4);
    header.version = 1;
    header.vertex_count = static_cast<e3d::u32>(mesh.vertices.size());
    header.index_count = static_cast<e3d::u32>(mesh.indices.size());
    header.vertex_stride = sizeof(e3d::MeshVertex);
    header.flags = 0x7;  // normals | uv | colour
    header.aabb_min[0] = aabb_min.x;
    header.aabb_min[1] = aabb_min.y;
    header.aabb_min[2] = aabb_min.z;
    header.aabb_max[0] = aabb_max.x;
    header.aabb_max[1] = aabb_max.y;
    header.aabb_max[2] = aabb_max.z;

    std::ofstream output(argv[2], std::ios::binary);
    if (!output) {
        std::fprintf(stderr, "meshc: cannot write %s\n", argv[2]);
        return 1;
    }

    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    output.write(reinterpret_cast<const char*>(mesh.vertices.data()),
                 static_cast<std::streamsize>(mesh.vertices.size() * sizeof(e3d::MeshVertex)));
    output.write(reinterpret_cast<const char*>(mesh.indices.data()),
                 static_cast<std::streamsize>(mesh.indices.size() * sizeof(e3d::u16)));

    std::printf("meshc: %zu vertices, %zu indices -> %s\n", mesh.vertices.size(),
                mesh.indices.size(), argv[2]);
    return 0;
}
