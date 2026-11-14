#include "project_scripts.hpp"

void register_project_components() {
    e3d::register_component(Spin::static_descriptor());
    e3d::register_component(PlaySoundOnButton::static_descriptor());
}
