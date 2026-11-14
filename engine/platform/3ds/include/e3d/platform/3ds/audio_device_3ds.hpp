#pragma once

#include "e3d/audio/audio.hpp"

#include <memory>

namespace e3d {

std::unique_ptr<AudioDevice> create_audio_device_3ds();

/// Result of the last ndspInit() call, for diagnostics.
u32 audio_device_3ds_last_result();

} // namespace e3d
