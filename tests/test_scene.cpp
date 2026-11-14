#include <doctest/doctest.h>

#include "e3d/core/guid.hpp"
#include "e3d/core/math.hpp"
#include "e3d/scene/components.hpp"
#include "e3d/scene/scene_io.hpp"
#include "e3d/scene/world.hpp"
#include "test_types.hpp"

#include <string>

using namespace e3d;

namespace {

const char* kScene = R"JSON({
  "format_version": 1,
  "name": "main",
  "root": {
    "guid": "0f8fad5b-d9cb-469f-a165-70867728950e",
    "name": "Main",
    "transform": { "position": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1] },
    "components": [
      { "type": "Environment", "props": { "clear_color": [0.04, 0.05, 0.08, 1.0], "ambient": 0.3 } },
      { "type": "Camera", "props": { "screen": "top", "field_of_view": 55, "near_plane": 0.1, "far_plane": 120, "stereo": true } },
      { "type": "FutureThing", "props": { "answer": 42 } }
    ],
    "children": [
      {
        "guid": "5c1f2b6e-3a84-4d1e-9b2f-1f7c2a8d9e31",
        "name": "Board",
        "transform": { "position": [0, 1, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1] },
        "components": [
          { "type": "TestMover", "props": {
              "speed": 4.5,
              "lane": 7,
              "label": "board",
              "target": { "$entity": "0f8fad5b-d9cb-469f-a165-70867728950e" },
              "future_field": { "nested": [1, 2, 3] }
          } }
        ],
        "children": []
      }
    ]
  }
})JSON";

} // namespace

TEST_CASE("scene loads entities, components, refs and unknown data") {
    register_test_components();

    World world;
    const SceneLoadResult result = load_scene(world, kScene);
    INFO(result.error);
    REQUIRE(result.ok);

    CHECK(world.name() == "main");
    CHECK(world.valid(result.root));

    const Entity* root = world.entity(result.root);
    REQUIRE(root != nullptr);
    CHECK(root->name == "Main");
    REQUIRE(root->children.size() == 1);

    const Environment* environment = world.find_component<Environment>(result.root);
    REQUIRE(environment != nullptr);
    CHECK(environment->ambient == doctest::Approx(0.3f));

    const Camera* camera = world.find_component<Camera>(result.root);
    REQUIRE(camera != nullptr);
    CHECK(camera->screen == ScreenTarget::Top);
    CHECK(camera->field_of_view == doctest::Approx(55.0f));
    CHECK(camera->stereo);

    const EntityId board_id = root->children[0];
    const Entity* board = world.entity(board_id);
    REQUIRE(board != nullptr);
    CHECK(board->name == "Board");

    const TestMover* mover = world.find_component<TestMover>(board_id);
    REQUIRE(mover != nullptr);
    CHECK(mover->speed == doctest::Approx(4.5f));
    CHECK(mover->lane == 7);
    CHECK(mover->active);
    CHECK(mover->uv.x == doctest::Approx(0.25f));
    CHECK(mover->label == "board");

    REQUIRE(mover->target.resolved());
    CHECK(mover->target.target == result.root);
    CHECK(guid_to_string(mover->target.guid) == "0f8fad5b-d9cb-469f-a165-70867728950e");

    REQUIRE(mover->extra_props.is_object());
    CHECK(mover->extra_props.find("future_field") != nullptr);

    CHECK(world.unresolved().size() == 1);
    REQUIRE_FALSE(result.warnings.empty());
    CHECK(result.warnings[0].find("FutureThing") != std::string::npos);

    world.update_transforms();
    const Vec3 board_origin = transform_point(world.entity(board_id)->world, {0.0f, 0.0f, 0.0f});
    CHECK(board_origin.y == doctest::Approx(1.0f));
}

TEST_CASE("scene save is deterministic and round-trips without data loss") {
    register_test_components();
    register_builtin_components();

    World world;
    REQUIRE(load_scene(world, kScene).ok);

    std::string error;
    const std::string first = save_scene(world, error);
    INFO(error);
    REQUIRE(error.empty());
    REQUIRE_FALSE(first.empty());

    CHECK(first.find("\"future_field\"") != std::string::npos);
    CHECK(first.find("\"FutureThing\"") != std::string::npos);
    CHECK(first.find("\"$entity\"") != std::string::npos);
    CHECK(first.find("\"target\"") != std::string::npos);

    World reloaded_world;
    const SceneLoadResult reloaded = load_scene(reloaded_world, first);
    INFO(reloaded.error);
    REQUIRE(reloaded.ok);

    std::string second_error;
    const std::string second = save_scene(reloaded_world, second_error);
    INFO(second_error);
    REQUIRE(second_error.empty());
    CHECK(second == first);

    CHECK(save_scene(world, error) == first);
}

TEST_CASE("scene reports unresolved entity references without failing") {
    register_test_components();

    const char* text = R"JSON({
      "format_version": 1,
      "name": "dangling",
      "root": {
        "guid": "0f8fad5b-d9cb-469f-a165-70867728950e",
        "name": "Main",
        "components": [
          { "type": "TestMover", "props": {
              "target": { "$entity": "11111111-2222-3333-4444-555555555555" }
          } }
        ]
      }
    })JSON";

    World world;
    const SceneLoadResult result = load_scene(world, text);
    INFO(result.error);
    REQUIRE(result.ok);
    REQUIRE_FALSE(result.warnings.empty());
    CHECK(result.warnings[0].find("unresolved reference") != std::string::npos);

    const TestMover* mover = world.find_component<TestMover>(result.root);
    REQUIRE(mover != nullptr);
    CHECK_FALSE(mover->target.resolved());
    CHECK(mover->target.guid.valid());

    std::string error;
    const std::string saved = save_scene(world, error);
    REQUIRE(error.empty());
    CHECK(saved.find("11111111-2222-3333-4444-555555555555") != std::string::npos);
}

TEST_CASE("scene failures carry a path to the offending property") {
    register_test_components();

    const char* text = R"JSON({
      "format_version": 1,
      "name": "broken",
      "root": {
        "guid": "0f8fad5b-d9cb-469f-a165-70867728950e",
        "name": "Main",
        "children": [
          {
            "guid": "5c1f2b6e-3a84-4d1e-9b2f-1f7c2a8d9e31",
            "name": "Board",
            "components": [
              { "type": "TestMover", "props": { "speed": "fast" } }
            ]
          }
        ]
      }
    })JSON";

    World world;
    const SceneLoadResult result = load_scene(world, text);
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("speed") != std::string::npos);
    CHECK(result.error.find("expected a number") != std::string::npos);
}

TEST_CASE("scene accepts flattened component properties and fills defaults") {
    register_test_components();

    const char* text = R"JSON({
      "format_version": 1,
      "name": "flat",
      "root": {
        "guid": "0f8fad5b-d9cb-469f-a165-70867728950e",
        "name": "Main",
        "components": [
          { "type": "TestMover", "speed": 3.0 },
          { "type": "Camera" }
        ]
      }
    })JSON";

    World world;
    const SceneLoadResult result = load_scene(world, text);
    INFO(result.error);
    REQUIRE(result.ok);

    const TestMover* mover = world.find_component<TestMover>(result.root);
    REQUIRE(mover != nullptr);
    CHECK(mover->speed == doctest::Approx(3.0f));
    CHECK(mover->lane == 3);
    CHECK(mover->uv.y == doctest::Approx(0.75f));

    const Camera* camera = world.find_component<Camera>(result.root);
    REQUIRE(camera != nullptr);
    CHECK(camera->screen == ScreenTarget::Top);
    CHECK(camera->near_plane == doctest::Approx(0.1f));
}

TEST_CASE("scene rejects malformed documents") {
    World world;

    auto fails = [&world](const char* text) {
        const SceneLoadResult result = load_scene(world, text);
        return !result.ok && !result.error.empty();
    };

    CHECK(fails("{"));
    CHECK(fails("[]"));
    CHECK(fails(R"({"format_version": 2, "root": {}})"));
    CHECK(fails(R"({"format_version": 1})"));
    CHECK(fails(R"({"format_version": 1, "root": {"guid": "nope"}})"));
    CHECK(fails(R"({"format_version": 1, "root": {"guid": "5c1f2b6e-3a84-4d1e-9b2f-1f7c2a8d9e31", "components": [{"type": "Camera", "props": {"screen": "sideways"}}]}})"));
}
