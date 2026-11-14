#include "e3d/audio/audio.hpp"

#include <vector>

namespace {

struct NullClip final {
    bool valid = false;
};

class NullAudioDevice final : public e3d::AudioDevice {
public:
    bool init() override { return true; }
    void shutdown() override { clips_.clear(); }

    e3d::ClipHandle load_wav(const void* data, std::size_t size) override {
        if (data == nullptr || size == 0) return {};
        ++next_clip_;
        return e3d::Handle<e3d::ClipTag>{next_clip_, 1};
    }

    void destroy_clip(e3d::ClipHandle) override {}

    e3d::VoiceHandle play(e3d::ClipHandle clip, const e3d::PlayParams&) override {
        if (!clip.valid()) return {};
        ++next_voice_;
        return e3d::Handle<e3d::VoiceTag>{next_voice_, 1};
    }

    void stop(e3d::VoiceHandle) override {}
    bool is_playing(e3d::VoiceHandle) const override { return false; }
    void set_master_volume(float) override {}
    void update() override {}

private:
    std::vector<NullClip> clips_;
    e3d::u32 next_clip_ = 0;
    e3d::u32 next_voice_ = 0;
};

} // namespace

namespace e3d {

std::unique_ptr<AudioDevice> create_null_audio_device() {
    return std::make_unique<NullAudioDevice>();
}

} // namespace e3d
