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

void register_test_components();
