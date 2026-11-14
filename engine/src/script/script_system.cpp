#include "e3d/script/script_system.hpp"

#include "e3d/reflect/registry.hpp"
#include "e3d/scene/world.hpp"
#include "e3d/script/script_component.hpp"

namespace e3d {

namespace {

template <typename Function>
void for_each_script(World& world, Function&& function) {
    for (const auto& slot : world.entities()) {
        if (slot == nullptr) continue;
        for (Component* component : slot->components) {
            const ComponentDesc* descriptor = component->descriptor();
            if (descriptor == nullptr || descriptor->kind != ComponentKind::Script) continue;
            function(*static_cast<ScriptComponent*>(component));
        }
    }
}

void ensure_started(ScriptComponent& script) {
    if (!script.created) {
        script.on_create();
        script.created = true;
    }
    if (!script.started && script.enabled) {
        script.on_start();
        script.started = true;
    }
}

} // namespace

void initialize_scripts(World& world) {
    for_each_script(world, [](ScriptComponent& script) {
        ensure_started(script);
        if (script.enabled && !script.enable_notified) {
            script.on_enable();
            script.enable_notified = true;
        }
    });
}

void update_scripts(World& world, float delta_time) {
    for_each_script(world, [delta_time](ScriptComponent& script) {
        if (!script.enabled) {
            if (script.enable_notified) {
                script.on_disable();
                script.enable_notified = false;
            }
            return;
        }

        ensure_started(script);
        if (!script.enable_notified) {
            script.on_enable();
            script.enable_notified = true;
        }
        script.on_update(delta_time);
    });
}

void fixed_update_scripts(World& world, float fixed_delta_time) {
    for_each_script(world, [fixed_delta_time](ScriptComponent& script) {
        if (!script.enabled) return;
        ensure_started(script);
        script.on_fixed_update(fixed_delta_time);
    });
}

void shutdown_scripts(World& world) {
    for_each_script(world, [](ScriptComponent& script) {
        if (!script.created) return;
        script.on_destroy();
        script.created = false;
        script.started = false;
    });
}

} // namespace e3d
