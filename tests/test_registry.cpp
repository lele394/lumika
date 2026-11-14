#include <doctest/doctest.h>

#include "e3d/core/guid.hpp"
#include "e3d/scene/components.hpp"
#include "e3d/scene/prop_io.hpp"
#include "e3d/scene/world.hpp"
#include "test_types.hpp"

#include <string_view>

using namespace e3d;

namespace {

const PropDesc* property(const ComponentDesc& descriptor, const char* name) {
    for (u32 i = 0; i < descriptor.prop_count; ++i) {
        if (std::string_view(descriptor.props[i].name) == name) return &descriptor.props[i];
    }
    return nullptr;
}

struct Fixture {
    World world;
    EntityId entity = world.create_entity("entity");
    TestMover* mover = world.add_component<TestMover>(entity);
};

} // namespace

TEST_CASE("component registry exposes descriptors by name and id") {
    register_test_components();

    const ComponentDesc* descriptor = find_component_desc("TestMover");
    REQUIRE(descriptor != nullptr);
    CHECK(descriptor->id == TestMover::static_type_id());
    CHECK(descriptor->kind == ComponentKind::Script);
    CHECK(descriptor->size == sizeof(TestMover));
    CHECK(descriptor->prop_count == 8);

    CHECK(find_component_desc(TestMover::static_type_id()) == descriptor);

    CHECK(property(*descriptor, "speed")->type == PropType::Float);
    CHECK(property(*descriptor, "lane")->type == PropType::Int);
    CHECK(property(*descriptor, "active")->type == PropType::Bool);
    CHECK(property(*descriptor, "uv")->type == PropType::Vec2);
    CHECK(property(*descriptor, "tint")->type == PropType::Vec4);
    CHECK(property(*descriptor, "label")->type == PropType::String);
    CHECK(property(*descriptor, "mesh")->type == PropType::AssetRef);
    CHECK(property(*descriptor, "target")->type == PropType::EntityRef);

    CHECK(property(*descriptor, "speed")->meta.min == doctest::Approx(-10.0f));
    CHECK(property(*descriptor, "speed")->meta.max == doctest::Approx(10.0f));
    CHECK(property(*descriptor, "mesh")->meta.asset_type != nullptr);
}

TEST_CASE("property codecs read every supported type") {
    register_test_components();
    Fixture fixture;
    REQUIRE(fixture.mover != nullptr);

    const ComponentDesc& descriptor = *fixture.mover->descriptor();

    json::Value props;
    json::ParseError parse_error;
    const char* text = R"({
        "speed": 4.5,
        "lane": 7,
        "active": false,
        "uv": [0.5, 0.25],
        "tint": [1.0, 0.5, 0.25, 0.75],
        "label": "board",
        "mesh": {"$asset": "0f8fad5b-d9cb-469f-a165-70867728950e"},
        "target": {"$entity": "5c1f2b6e-3a84-4d1e-9b2f-1f7c2a8d9e31"}
    })";
    REQUIRE(json::parse(text, props, parse_error));

    std::string error;
    for (std::size_t i = 0; i < props.size(); ++i) {
        const PropDesc* desc = property(descriptor, props.key_at(i).c_str());
        REQUIRE(desc != nullptr);
        REQUIRE(read_property(*fixture.mover, *desc, props.value_at(i), error));
    }

    CHECK(fixture.mover->speed == doctest::Approx(4.5f));
    CHECK(fixture.mover->lane == 7);
    CHECK_FALSE(fixture.mover->active);
    CHECK(fixture.mover->uv.x == doctest::Approx(0.5f));
    CHECK(fixture.mover->tint.w == doctest::Approx(0.75f));
    CHECK(fixture.mover->label == "board");
    CHECK(guid_to_string(fixture.mover->mesh.guid) == "0f8fad5b-d9cb-469f-a165-70867728950e");
    CHECK(guid_to_string(fixture.mover->target.guid) == "5c1f2b6e-3a84-4d1e-9b2f-1f7c2a8d9e31");
    CHECK_FALSE(fixture.mover->target.resolved());

    const json::Value written = write_property(*fixture.mover, *property(descriptor, "speed"),
                                               &fixture.world);
    CHECK(written.as_number() == doctest::Approx(4.5));

    const json::Value reference =
        write_property(*fixture.mover, *property(descriptor, "target"), &fixture.world);
    CHECK(reference.find("$entity")->as_string() == "5c1f2b6e-3a84-4d1e-9b2f-1f7c2a8d9e31");
}

TEST_CASE("enum properties map names to values") {
    register_builtin_components();
    World world;
    const EntityId id = world.create_entity("camera");
    Camera* camera = world.add_component<Camera>(id);
    REQUIRE(camera != nullptr);

    const PropDesc* screen = property(*camera->descriptor(), "screen");
    REQUIRE(screen != nullptr);
    CHECK(screen->type == PropType::Enum);

    json::Value value = json::Value::string("bottom");
    std::string error;
    REQUIRE(read_property(*camera, *screen, value, error));
    CHECK(camera->screen == ScreenTarget::Bottom);

    const json::Value written = write_property(*camera, *screen, &world);
    REQUIRE(written.is_string());
    CHECK(written.as_string() == "bottom");

    json::Value invalid = json::Value::string("sideways");
    CHECK_FALSE(read_property(*camera, *screen, invalid, error));
    CHECK(camera->screen == ScreenTarget::Bottom);
}

TEST_CASE("property type errors are reported and leave values untouched") {
    register_test_components();
    Fixture fixture;
    const PropDesc* speed = property(*fixture.mover->descriptor(), "speed");
    REQUIRE(speed != nullptr);

    const float before = fixture.mover->speed;
    json::Value value = json::Value::string("fast");

    std::string error;
    CHECK_FALSE(read_property(*fixture.mover, *speed, value, error));
    CHECK_FALSE(error.empty());
    CHECK(fixture.mover->speed == before);
}

TEST_CASE("entity refs recover guids when written") {
    World world;
    const EntityId target = world.create_entity("target");
    Environment* environment = world.add_component<Environment>(target);
    REQUIRE(environment != nullptr);

    EntityRef reference;
    reference.target = target;

    TestMover mover;
    mover.target = reference;

    const PropDesc* desc = property(*TestMover::static_descriptor(), "target");
    REQUIRE(desc != nullptr);

    const json::Value written = write_property(mover, *desc, &world);
    REQUIRE(written.is_object());
    CHECK(written.find("$entity")->as_string() == guid_to_string(world.guid_of(target)));
}
