#pragma once

#include "e3d/core/types.hpp"

namespace e3d {

enum class Button : u8 {
    A,
    B,
    X,
    Y,
    L,
    R,
    ZL,
    ZR,
    Start,
    Select,
    Up,
    Down,
    Left,
    Right,
    Touch,
    Count,
};

inline constexpr u32 button_bit(Button button) noexcept {
    return 1u << static_cast<u32>(button);
}

struct InputState final {
    u32 held = 0;
    u32 pressed = 0;
    u32 released = 0;

    Vec2 circle_pad;
    Vec2 c_stick;

    bool touch_down = false;
    Vec2 touch;

    u32 raw_hid = 0;

    [[nodiscard]] bool is_held(Button button) const noexcept {
        return (held & button_bit(button)) != 0;
    }
    [[nodiscard]] bool is_pressed(Button button) const noexcept {
        return (pressed & button_bit(button)) != 0;
    }
    [[nodiscard]] bool is_released(Button button) const noexcept {
        return (released & button_bit(button)) != 0;
    }
};

/// Converts libctru HID state into the portable InputState. Call after hidScanInput().
void poll_input_3ds(InputState& out);

} // namespace e3d
