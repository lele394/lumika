#pragma once

#include "e3d/reflect/registry.hpp"
#include "e3d/script/script_component.hpp"

#include <span>
#include <string>

/// A component exercising every supported property type. Defined once for the
/// whole test binary and registered explicitly by `register_test_components()`.
class TestMover final : public e3d::ScriptComponent {
public:
    E3D_COMPONENT(TestMover, e3d::ScriptComponent)

    float speed = 2.5f;
    int lane = 3;
    bool active = true;
    e3d::Vec2 uv{0.25f, 0.75f};
    e3d::Vec4 tint{1.0f, 0.5f, 0.25f, 1.0f};
    std::string label = "hello";
    e3d::AssetRef mesh;
    e3d::EntityRef target;

    static std::span<const e3d::PropDesc> properties();
};

/// Records lifecycle calls in static counters so destruction can be checked.
class LifecycleProbe final : public e3d::ScriptComponent {
public:
    E3D_COMPONENT(LifecycleProbe, e3d::ScriptComponent)

    static int created;
    static int started;
    static int enabled_count;
    static int disabled;
    static int updates;
    static int fixed_updates;
    static int destroyed;

    static void reset();
    static std::span<const e3d::PropDesc> properties();

    void on_create() override;
    void on_start() override;
    void on_enable() override;
    void on_disable() override;
    void on_update(float delta_time) override;
    void on_fixed_update(float fixed_delta_time) override;
    void on_destroy() override;
};

void register_test_components();
