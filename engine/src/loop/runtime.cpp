#include "e3d/loop/runtime.hpp"

#include "e3d/audio/audio.hpp"
#include "e3d/input/input.hpp"
#include "e3d/render/render_list.hpp"
#include "e3d/script/script_context.hpp"
#include "e3d/script/script_system.hpp"

namespace e3d {

Runtime::Runtime(World& world, AudioDevice* audio) : world_(world), audio_(audio) {
    initialize_scripts(world_);
}

void Runtime::update(const InputState& input_state, float raw_delta_time) {
    float delta_time = raw_delta_time * time_scale_;
    if (delta_time < 0.0f) delta_time = 0.0f;
    if (delta_time > 0.1f) delta_time = 0.1f;

    ScriptContext& context = script_context();
    context.world = &world_;
    context.input = &input_state;
    context.audio = audio_;
    context.render_list = render_list_;
    context.render_device = render_device_;
    context.delta_time = delta_time;
    context.fixed_delta_time = fixed_delta_time_;

    if (!paused_) {
        accumulator_ += delta_time;

        int steps = 0;
        while (accumulator_ >= fixed_delta_time_ && steps < 4) {
            fixed_update_scripts(world_, fixed_delta_time_);
            accumulator_ -= fixed_delta_time_;
            ++steps;
        }

        update_scripts(world_, delta_time);
        elapsed_ += delta_time;
    }

    ++frame_;
    if (audio_ != nullptr) audio_->update();
}

} // namespace e3d
