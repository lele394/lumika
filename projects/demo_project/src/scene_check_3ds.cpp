// M1 device check: loads a scene through the real reflection/serialization path,
// verifies references and round-tripping, then writes a report to the SD card so
// the result can be inspected from the host.

#include <3ds.h>

#include <cstdio>
#include <span>
#include <string>

#include "e3d/e3d.hpp"

namespace {

class Probe final : public e3d::ScriptComponent {
public:
    E3D_COMPONENT(Probe, e3d::ScriptComponent)

    e3d::EntityRef target;

    static std::span<const e3d::PropDesc> properties();
};

std::span<const e3d::PropDesc> Probe::properties() {
    static const e3d::PropDesc props[] = {
        E3D_PROP(Probe, target),
    };
    return {props, std::size(props)};
}

E3D_IMPLEMENT_COMPONENT(Probe)

const char* kScene = R"JSON({
  "format_version": 1,
  "name": "device",
  "root": {
    "guid": "0f8fad5b-d9cb-469f-a165-70867728950e",
    "name": "Root",
    "components": [
      { "type": "Probe", "props": {
          "target": { "$entity": "5c1f2b6e-3a84-4d1e-9b2f-1f7c2a8d9e31" },
          "legacy": true
      } }
    ],
    "children": [
      {
        "guid": "5c1f2b6e-3a84-4d1e-9b2f-1f7c2a8d9e31",
        "name": "Child",
        "transform": { "position": [0, 2, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1] },
        "components": [
          { "type": "MeshRenderer", "props": {
              "mesh": { "$asset": "2f0a3c4d-5e6f-4a7b-8c9d-0e1f2a3b4c5d" }
          } }
        ]
      }
    ]
  }
})JSON";

void write_report(const char* text) {
    FILE* file = std::fopen("sdmc:/3ds-engine-scene-check.txt", "w");
    if (file == nullptr) return;
    std::fputs(text, file);
    std::fclose(file);
}

} // namespace

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, nullptr);

    e3d::register_component(Probe::static_descriptor());

    e3d::World world;
    const e3d::SceneLoadResult result = e3d::load_scene(world, kScene);

    char report[1024] = {};
    char console_text[1200] = {};

    if (!result.ok) {
        std::snprintf(report, sizeof(report), "FAIL\n%s\n", result.error.c_str());
    } else {
        world.update_transforms();

        const e3d::EntityId child_id = world.entity(result.root) != nullptr &&
                                               !world.entity(result.root)->children.empty()
                                           ? world.entity(result.root)->children[0]
                                           : e3d::kInvalidEntity;

        Probe* probe = world.find_component<Probe>(result.root);
        const e3d::Entity* child = world.entity(child_id);

        const float child_y = child != nullptr
                                  ? e3d::transform_point(child->world, {0.0f, 0.0f, 0.0f}).y
                                  : -1.0f;

        std::string save_error;
        const std::string saved = e3d::save_scene(world, save_error);

        e3d::World reloaded;
        const e3d::SceneLoadResult second = e3d::load_scene(reloaded, saved);
        std::string second_error;
        const std::string saved_again = e3d::save_scene(reloaded, second_error);

        std::snprintf(report, sizeof(report),
                      "scene=%s\n"
                      "entities=%u\n"
                      "target_resolved=%d\n"
                      "legacy_preserved=%d\n"
                      "asset_preserved=%d\n"
                      "child_world_y_milli=%d\n"
                      "reload_ok=%d\n"
                      "roundtrip=%s\n"
                      "warnings=%u\n",
                      world.name().c_str(), static_cast<unsigned>(world.entities().size()),
                      probe != nullptr && probe->target.resolved() ? 1 : 0,
                      probe != nullptr && probe->extra_props.find("legacy") != nullptr ? 1 : 0,
                      saved.find("2f0a3c4d-5e6f-4a7b-8c9d-0e1f2a3b4c5d") != std::string::npos ? 1
                                                                                              : 0,
                      static_cast<int>(child_y * 1000.0f), second.ok ? 1 : 0,
                      saved == saved_again ? "identical" : "DIFFERENT",
                      static_cast<unsigned>(result.warnings.size()));
    }

    std::snprintf(console_text, sizeof(console_text), "3ds-engine M1 scene check\n\n%s\n\nSTART exits\n",
                  report);

    std::printf("%s", console_text);
    write_report(report);

    for (int frame = 0; frame < 300 && aptMainLoop(); ++frame) {
        hidScanInput();
        if ((hidKeysDown() & KEY_START) != 0) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
