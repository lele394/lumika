#pragma once

#include "e3d/core/types.hpp"

#include <cstddef>

namespace e3d::wav {

struct WavInfo final {
    const i16* samples = nullptr;
    u32 sample_frames = 0;
    u16 channels = 0;
    u32 sample_rate = 0;
};

/// Parses a RIFF/WAVE PCM16 file. Points into the supplied buffer.
bool parse_wav(const void* data, std::size_t size, WavInfo& out);

} // namespace e3d::wav
