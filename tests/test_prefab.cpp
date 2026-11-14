#include <doctest/doctest.h>

#include "e3d/scene/components.hpp"
#include "e3d/scene/scene_io.hpp"
#include "e3d/scene/world.hpp"
#include "test_types.hpp"

using namespace e3d;

TEST_CASE("entity subtrees serialize and instantiate with fresh guids") {
    register_test_components();
    register_builtin_components();

    World source;
    const EntityId root = source.create_entity("Root");
    source.add_component<Environment>(root);
    const EntityId child = source.create_entity("Child", root);
    TestMover* mover = source.add_component<TestMover>(child);
    mover->label = "child";
    mover->target.guid = source.guid_of(root);
    mover->target.target = root;

    const std::string prefab = save_entity_json(source, root);
    REQUIRE_FALSE(prefab.empty());

    World targetWorld;
    std::string error;
    const EntityId instance = instantiate_prefab(targetWorld, prefab, kInvalidEntity, error);
    INFO(error);
    REQUIRE(instance != kInvalidEntity);

    const Entity* instanceEntity = targetWorld.entity(instance);
    REQUIRE(instanceEntity != nullptr);
    CHECK(instanceEntity->name == "Root");
    CHECK(instanceEntity->guid != source.entity(root)->guid);
    REQUIRE(instanceEntity->children.size() == 1);

    const EntityId instanceChild = instanceEntity->children[0];
    const TestMover* instanceMover = targetWorld.find_component<TestMover>(instanceChild);
    REQUIRE(instanceMover != nullptr);
    CHECK(instanceMover->label == "child");
    REQUIRE(instanceMover->target.resolved());
    CHECK(instanceMover->target.target == instance);
}

TEST_CASE("prefabs can be instantiated more than once") {
    register_test_components();
    register_builtin_components();

    World source;
    const EntityId root = source.create_entity("Root");
    source.create_entity("Child", root);
    const std::string prefab = save_entity_json(source, root);

    World targetWorld;
    std::string error;
    const EntityId first = instantiate_prefab(targetWorld, prefab, kInvalidEntity, error);
    const EntityId second = instantiate_prefab(targetWorld, prefab, kInvalidEntity, error);
    REQUIRE(first != kInvalidEntity);
    REQUIRE(second != kInvalidEntity);
    CHECK(first != second);
    CHECK(targetWorld.entity(first)->guid != targetWorld.entity(second)->guid);
}
