#include <doctest/doctest.h>

#include "e3d/core/guid.hpp"
#include "e3d/core/math.hpp"
#include "e3d/scene/components.hpp"
#include "e3d/scene/world.hpp"

using namespace e3d;

TEST_CASE("entity pool recycles indices with new generations") {
    World world;
    const EntityId a = world.create_entity("a");
    REQUIRE(world.valid(a));
    CHECK(entity_index(a) == 0);

    const EntityId b = world.create_entity("b", a);
    REQUIRE(world.valid(b));
    REQUIRE(world.entity(a) != nullptr);
    CHECK(world.entity(a)->children.size() == 1);

    world.destroy_entity(a);
    CHECK_FALSE(world.valid(a));
    CHECK_FALSE(world.valid(b));

    const EntityId c = world.create_entity("c");
    REQUIRE(world.valid(c));
    CHECK(entity_index(c) == entity_index(a));
    CHECK(entity_generation(c) != entity_generation(a));
    CHECK(c != a);
}

TEST_CASE("hierarchy composes world matrices") {
    World world;
    const EntityId root = world.create_entity("root");
    const EntityId child = world.create_entity("child", root);

    world.entity(root)->transform.position = {10.0f, 0.0f, 0.0f};
    world.entity(child)->transform.position = {0.0f, 5.0f, 0.0f};

    world.update_transforms();
    const Vec3 origin = transform_point(world.entity(child)->world, {0.0f, 0.0f, 0.0f});
    CHECK(origin.x == doctest::Approx(10.0f));
    CHECK(origin.y == doctest::Approx(5.0f));

    world.entity(root)->transform.scale = {2.0f, 2.0f, 2.0f};
    world.update_transforms();
    const Vec3 scaled = transform_point(world.entity(child)->world, {0.0f, 0.0f, 0.0f});
    CHECK(scaled.x == doctest::Approx(10.0f));
    CHECK(scaled.y == doctest::Approx(10.0f));
}

TEST_CASE("set_parent moves children and rejects cycles") {
    World world;
    const EntityId a = world.create_entity("a");
    const EntityId b = world.create_entity("b", a);
    const EntityId c = world.create_entity("c", b);

    // Making an ancestor a child of its own descendant must be ignored.
    world.set_parent(a, c);
    CHECK(world.entity(a)->parent == kInvalidEntity);
    CHECK(world.entity(b)->parent == a);
    CHECK(world.entity(c)->parent == b);

    world.set_parent(c, kInvalidEntity);
    CHECK(world.entity(c)->parent == kInvalidEntity);
    CHECK(world.entity(a)->children.size() == 1);
    CHECK(world.entity(b)->children.empty());
}

TEST_CASE("components attach, resolve and detach") {
    World world;
    const EntityId id = world.create_entity("entity");

    Environment* environment = world.add_component<Environment>(id);
    REQUIRE(environment != nullptr);
    CHECK(environment->entity() == id);
    CHECK(world.find_component<Environment>(id) == environment);
    CHECK(world.find_component<Camera>(id) == nullptr);

    Camera* camera = world.add_component<Camera>(id);
    REQUIRE(camera != nullptr);
    CHECK(camera->descriptor()->kind == ComponentKind::Engine);

    world.remove_component(environment);
    CHECK(world.find_component<Environment>(id) == nullptr);
    CHECK(world.find_component<Camera>(id) == camera);
}

TEST_CASE("clear removes entities and components") {
    World world;
    const EntityId id = world.create_entity("entity");
    world.add_component<Environment>(id);

    world.clear();
    CHECK(world.entities().empty());
    CHECK(world.root() == kInvalidEntity);
    CHECK_FALSE(world.valid(id));
}

TEST_CASE("guid lookup finds entities") {
    World world;
    const Guid guid = generate_guid();
    const EntityId id = world.create_entity_with_guid(guid, "named");

    CHECK(world.find_by_guid(guid) == id);
    CHECK(world.guid_of(id) == guid);
    CHECK(world.find_by_guid(generate_guid()) == kInvalidEntity);
}
