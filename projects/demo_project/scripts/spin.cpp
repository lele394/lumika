#include "project_scripts.hpp"

std::span<const e3d::PropDesc> Spin::properties() {
    static const e3d::PropDesc props[] = {
        E3D_PROP(Spin, axis),
        E3D_PROP(Spin, degrees_per_second).step(1.0f),
    };
    return {props, std::size(props)};
}

void Spin::on_update(float delta_time) {
    e3d::Entity* owner = e3d::world().entity(entity());
    if (owner == nullptr) return;
    owner->transform.rotation += axis * (degrees_per_second * delta_time);
}

E3D_IMPLEMENT_COMPONENT(Spin)
