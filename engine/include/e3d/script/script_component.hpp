#pragma once

#include "e3d/scene/component.hpp"

namespace e3d {

/// Base class for project-defined behaviour components.
class ScriptComponent : public Component {
public:
    virtual void on_create() {}
    virtual void on_awake() {}
    virtual void on_enable() {}
    virtual void on_start() {}
    virtual void on_update(float /*delta_time*/) {}
    virtual void on_fixed_update(float /*fixed_delta_time*/) {}
    virtual void on_disable() {}
    virtual void on_destroy() {}

    bool created = false;
    bool started = false;
    bool enable_notified = false;
};

} // namespace e3d
