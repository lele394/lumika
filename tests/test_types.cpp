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

int LifecycleProbe::created = 0;
int LifecycleProbe::started = 0;
int LifecycleProbe::enabled_count = 0;
int LifecycleProbe::disabled = 0;
int LifecycleProbe::updates = 0;
int LifecycleProbe::fixed_updates = 0;
int LifecycleProbe::destroyed = 0;

void LifecycleProbe::reset() {
    created = 0;
    started = 0;
    enabled_count = 0;
    disabled = 0;
    updates = 0;
    fixed_updates = 0;
    destroyed = 0;
}

std::span<const e3d::PropDesc> LifecycleProbe::properties() { return {}; }

void LifecycleProbe::on_create() { ++created; }
void LifecycleProbe::on_start() { ++started; }
void LifecycleProbe::on_enable() { ++enabled_count; }
void LifecycleProbe::on_disable() { ++disabled; }
void LifecycleProbe::on_update(float) { ++updates; }
void LifecycleProbe::on_fixed_update(float) { ++fixed_updates; }
void LifecycleProbe::on_destroy() { ++destroyed; }

E3D_IMPLEMENT_COMPONENT(LifecycleProbe)

void register_test_components() {
    e3d::register_component(TestMover::static_descriptor());
    e3d::register_component(LifecycleProbe::static_descriptor());
}
