#pragma once

#include "e3d/core/types.hpp"

namespace e3d {

class World;
class AudioDevice;
class RenderList;
class RenderDevice;
struct InputState;

/// Per-frame services exposed to scripts. Set by the runtime; scripts should use
/// the accessor functions below rather than touching the struct directly.
struct ScriptContext final {
    World* world = nullptr;
    const InputState* input = nullptr;
    AudioDevice* audio = nullptr;
    RenderList* render_list = nullptr;
    RenderDevice* render_device = nullptr;
    float delta_time = 0.0f;
    float fixed_delta_time = 1.0f / 60.0f;
};

ScriptContext& script_context();

World& world();
const InputState& input();
AudioDevice& audio();
RenderList& render_list();
RenderDevice& render_device();

} // namespace e3d
