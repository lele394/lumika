#include "e3d/script/script_context.hpp"

#include "e3d/audio/audio.hpp"
#include "e3d/core/log.hpp"
#include "e3d/input/input.hpp"
#include "e3d/render/render_list.hpp"
#include "e3d/scene/world.hpp"

namespace e3d {

ScriptContext& script_context() {
    static ScriptContext context;
    return context;
}

World& world() {
    E3D_ASSERT(script_context().world != nullptr);
    return *script_context().world;
}

const InputState& input() {
    static const InputState empty;
    const InputState* state = script_context().input;
    return state != nullptr ? *state : empty;
}

AudioDevice& audio() {
    E3D_ASSERT(script_context().audio != nullptr);
    return *script_context().audio;
}

RenderList& render_list() {
    E3D_ASSERT(script_context().render_list != nullptr);
    return *script_context().render_list;
}

} // namespace e3d
