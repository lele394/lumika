#include <doctest/doctest.h>

#include "e3d/input/input.hpp"
#include "e3d/loop/runtime.hpp"
#include "e3d/scene/world.hpp"
#include "e3d/script/script_system.hpp"
#include "test_types.hpp"

using namespace e3d;

TEST_CASE("script lifecycle dispatches in order and respects enable") {
    register_test_components();
    LifecycleProbe::reset();

    World world;
    const EntityId entity = world.create_entity("probe");
    LifecycleProbe* probe = world.add_component<LifecycleProbe>(entity);
    REQUIRE(probe != nullptr);

    initialize_scripts(world);
    CHECK(LifecycleProbe::created == 1);
    CHECK(LifecycleProbe::started == 1);
    CHECK(LifecycleProbe::enabled_count == 1);

    update_scripts(world, 0.1f);
    CHECK(LifecycleProbe::updates == 1);

    fixed_update_scripts(world, 1.0f / 60.0f);
    CHECK(LifecycleProbe::fixed_updates == 1);

    probe->enabled = false;
    update_scripts(world, 0.1f);
    CHECK(LifecycleProbe::disabled == 1);
    CHECK(LifecycleProbe::updates == 1);

    fixed_update_scripts(world, 1.0f / 60.0f);
    CHECK(LifecycleProbe::fixed_updates == 1);

    probe->enabled = true;
    update_scripts(world, 0.1f);
    CHECK(LifecycleProbe::enabled_count == 2);
    CHECK(LifecycleProbe::updates == 2);

    world.destroy_entity(entity);
    CHECK(LifecycleProbe::destroyed == 1);
}

TEST_CASE("runtime drives fixed steps and pauses") {
    register_test_components();
    LifecycleProbe::reset();

    World world;
    world.add_component<LifecycleProbe>(world.create_entity("probe"));

    Runtime runtime(world, nullptr);
    InputState input;

    runtime.update(input, 1.0f / 30.0f);
    CHECK(LifecycleProbe::updates == 1);
    CHECK(LifecycleProbe::fixed_updates == 2);

    runtime.set_paused(true);
    runtime.update(input, 1.0f / 30.0f);
    CHECK(LifecycleProbe::updates == 1);
    CHECK(LifecycleProbe::fixed_updates == 2);

    runtime.set_paused(false);
    runtime.update(input, 1.0f / 60.0f);
    CHECK(LifecycleProbe::updates == 2);
}

TEST_CASE("input state reports edges") {
    InputState input;
    input.held = button_bit(Button::A);
    input.pressed = button_bit(Button::A);
    input.released = button_bit(Button::B);

    CHECK(input.is_held(Button::A));
    CHECK(input.is_pressed(Button::A));
    CHECK(input.is_released(Button::B));
    CHECK_FALSE(input.is_held(Button::B));
    CHECK_FALSE(input.is_pressed(Button::B));
}
