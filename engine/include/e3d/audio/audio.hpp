#pragma once

#include "e3d/core/types.hpp"

#include <cstddef>
#include <memory>

namespace e3d {

struct ClipTag;
struct VoiceTag;

using ClipHandle = Handle<ClipTag>;
using VoiceHandle = Handle<VoiceTag>;

struct PlayParams final {
    float volume = 1.0f;
    float pan = 0.0f;  // -1 left, +1 right
    float rate = 1.0f;
    bool loop = false;
};

/// Platform audio backend. Clips are decoded WAV PCM, resident in memory.
class AudioDevice {
public:
    virtual ~AudioDevice() = default;

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    virtual ClipHandle load_wav(const void* data, std::size_t size) = 0;
    virtual void destroy_clip(ClipHandle clip) = 0;

    virtual VoiceHandle play(ClipHandle clip, const PlayParams& params) = 0;
    virtual void stop(VoiceHandle voice) = 0;
    virtual bool is_playing(VoiceHandle voice) const = 0;

    virtual void set_master_volume(float volume) = 0;
    virtual void update() = 0;
};

/// Silent backend for host builds and tests; tracks clips but produces no output.
std::unique_ptr<AudioDevice> create_null_audio_device();

} // namespace e3d
