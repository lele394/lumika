#include "e3d/input/input.hpp"

#include <3ds.h>

namespace {

float clamp_axis(float value, float limit) {
    if (value > limit) return limit;
    if (value < -limit) return -limit;
    return value / limit;
}

struct Mapping final {
    e3d::Button button;
    u32 key;
};

constexpr Mapping kMappings[] = {
    {e3d::Button::A, KEY_A},         {e3d::Button::B, KEY_B},
    {e3d::Button::X, KEY_X},         {e3d::Button::Y, KEY_Y},
    {e3d::Button::L, KEY_L},         {e3d::Button::R, KEY_R},
    {e3d::Button::ZL, KEY_ZL},       {e3d::Button::ZR, KEY_ZR},
    {e3d::Button::Start, KEY_START}, {e3d::Button::Select, KEY_SELECT},
    {e3d::Button::Up, KEY_DUP},      {e3d::Button::Down, KEY_DDOWN},
    {e3d::Button::Left, KEY_DLEFT},  {e3d::Button::Right, KEY_DRIGHT},
    {e3d::Button::Touch, KEY_TOUCH},
};

} // namespace

namespace e3d {

void poll_input_3ds(InputState& out) {
    out = {};

    const u32 held = hidKeysHeld();
    const u32 down = hidKeysDown();
    const u32 up = hidKeysUp();

    for (const Mapping& mapping : kMappings) {
        const u32 bit = button_bit(mapping.button);
        if ((held & mapping.key) != 0) out.held |= bit;
        if ((down & mapping.key) != 0) out.pressed |= bit;
        if ((up & mapping.key) != 0) out.released |= bit;
    }

    circlePosition circle{};
    hidCircleRead(&circle);
    out.circle_pad = Vec2{clamp_axis(static_cast<float>(circle.dx), 156.0f),
                          clamp_axis(static_cast<float>(circle.dy), 156.0f)};

    circlePosition cstick{};
    hidCstickRead(&cstick);
    out.c_stick = Vec2{clamp_axis(static_cast<float>(cstick.dx), 156.0f),
                       clamp_axis(static_cast<float>(cstick.dy), 156.0f)};

    touchPosition touch{};
    hidTouchRead(&touch);
    out.touch_down = (held & KEY_TOUCH) != 0;
    out.touch = Vec2{static_cast<float>(touch.px), static_cast<float>(touch.py)};
    out.raw_hid = held;
}

} // namespace e3d
