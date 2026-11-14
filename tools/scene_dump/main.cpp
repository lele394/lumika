#include "e3d/core/guid.hpp"
#include "e3d/scene/scene_io.hpp"
#include "e3d/scene/world.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

void print_entity(const e3d::World& world, const e3d::Entity& entity, int depth) {
    std::string indent(static_cast<std::size_t>(depth) * 2, ' ');

    std::printf("%s%s  [%s]\n", indent.c_str(), entity.name.c_str(),
                e3d::guid_to_string(entity.guid).c_str());

    for (const e3d::Component* component : entity.components) {
        const e3d::ComponentDesc* descriptor = component->descriptor();
        std::printf("%s  - %s\n", indent.c_str(), descriptor != nullptr ? descriptor->name : "?");
    }

    for (e3d::EntityId child_id : entity.children) {
        const e3d::Entity* child = world.entity(child_id);
        if (child != nullptr) print_entity(world, *child, depth + 1);
    }
}

bool read_file(const char* path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: scene_dump <scene.json> [output.json]\n");
        return 1;
    }

    std::string text;
    if (!read_file(argv[1], text)) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }

    e3d::World world;
    const e3d::SceneLoadResult result = e3d::load_scene(world, text);

    for (const std::string& warning : result.warnings) {
        std::fprintf(stderr, "warning: %s\n", warning.c_str());
    }

    if (!result.ok) {
        std::fprintf(stderr, "error: %s\n", result.error.c_str());
        return 2;
    }

    std::printf("scene '%s' (%zu entities)\n", world.name().c_str(), world.entities().size());
    if (const e3d::Entity* root = world.entity(result.root)) print_entity(world, *root, 0);

    if (argc >= 3) {
        std::string error;
        const std::string saved = e3d::save_scene(world, error);
        if (saved.empty()) {
            std::fprintf(stderr, "error: %s\n", error.c_str());
            return 2;
        }

        std::ofstream file(argv[2], std::ios::binary);
        if (!file) {
            std::fprintf(stderr, "cannot write %s\n", argv[2]);
            return 1;
        }
        file << saved;
        std::printf("wrote %s\n", argv[2]);
    }

    return 0;
}
