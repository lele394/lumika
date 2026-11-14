#pragma once

#include "e3d/core/types.hpp"

namespace e3d {

class World;
class AudioDevice;
class RenderList;
class RenderDevice;
struct InputState;

/// Owns time, fixed-step scheduling and script dispatch. The app calls update()
/// once per frame and then builds/executes the render list.
class Runtime final {
public:
    Runtime(World& world, AudioDevice* audio);

    void set_render_list(RenderList* list) noexcept { render_list_ = list; }
    void set_render_device(RenderDevice* device) noexcept { render_device_ = device; }

    void update(const InputState& input, float raw_delta_time);

    void set_paused(bool paused) noexcept { paused_ = paused; }
    [[nodiscard]] bool paused() const noexcept { return paused_; }

    void set_time_scale(float scale) noexcept { time_scale_ = scale; }
    [[nodiscard]] float time_scale() const noexcept { return time_scale_; }

    [[nodiscard]] float fixed_delta_time() const noexcept { return fixed_delta_time_; }
    [[nodiscard]] double elapsed() const noexcept { return elapsed_; }
    [[nodiscard]] u64 frame() const noexcept { return frame_; }

private:
    World& world_;
    AudioDevice* audio_;
    RenderList* render_list_ = nullptr;
    RenderDevice* render_device_ = nullptr;
    float fixed_delta_time_ = 1.0f / 60.0f;
    float accumulator_ = 0.0f;
    float time_scale_ = 1.0f;
    double elapsed_ = 0.0;
    u64 frame_ = 0;
    bool paused_ = false;
};

} // namespace e3d
