#include "test_types.hpp"

#include <iterator>

std::span<const e3d::PropDesc> TestMover::properties() {
    static const e3d::PropDesc props[] = {
        E3D_PROP(TestMover, speed).range(-10.0f, 10.0f).step(0.1f),
        E3D_PROP(TestMover, lane),
        E3D_PROP(TestMover, active),
        E3D_PROP(TestMover, uv),
        E3D_PROP(TestMover, tint),
        E3D_PROP(TestMover, label),
        E3D_PROP(TestMover, mesh).asset_type("mesh"),
        E3D_PROP(TestMover, target),
    };
    return {props, std::size(props)};
}

E3D_IMPLEMENT_COMPONENT(TestMover)

void register_test_components() { e3d::register_component(TestMover::static_descriptor()); }
