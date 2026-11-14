#include "e3d/platform/3ds/audio_device_3ds.hpp"

#include "e3d/audio/wav.hpp"
#include "e3d/render/handle_pool.hpp"

#include <3ds.h>

#include <cstring>
#include <memory>

namespace {

constexpr int kChannelCount = 8;

Result g_last_init_result = 0;

struct ClipSlot final {
    void* data = nullptr;
    u32 sample_frames = 0;
    u16 channels = 0;
    u32 sample_rate = 0;
    bool valid = false;
};

struct VoiceSlot final {
    u32 generation = 0;
    e3d::ClipHandle clip;
    bool active = false;
};

class AudioDevice3DS final : public e3d::AudioDevice {
public:
    ~AudioDevice3DS() override { shutdown(); }

    bool init() override {
        if (initialized_) return true;
        g_last_init_result = ndspInit();
        if (R_FAILED(g_last_init_result)) return false;

        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
        ndspSetMasterVol(1.0f);

        for (int channel = 0; channel < kChannelCount; ++channel) {
            ndspChnReset(channel);
            ndspChnSetInterp(channel, NDSP_INTERP_LINEAR);
            ndspChnSetRate(channel, 32000.0f);
            ndspChnSetFormat(channel, NDSP_FORMAT_STEREO_PCM16);
            set_mix(channel, 0.0f, 0.0f);
            std::memset(&wave_buffers_[channel], 0, sizeof(ndspWaveBuf));
            voices_[channel] = VoiceSlot{};
        }

        initialized_ = true;
        return true;
    }

    void shutdown() override {
        if (!initialized_) return;
        initialized_ = false;

        for (int channel = 0; channel < kChannelCount; ++channel) {
            ndspChnWaveBufClear(channel);
            ndspChnReset(channel);
        }
        ndspExit();

        clips_.for_each([](ClipSlot& clip) {
            if (clip.data != nullptr) {
                linearFree(clip.data);
                clip.data = nullptr;
            }
            clip.valid = false;
        });
    }

    e3d::ClipHandle load_wav(const void* data, std::size_t size) override {
        e3d::wav::WavInfo wav;
        if (!e3d::wav::parse_wav(data, size, wav)) return {};

        const u32 bytes = wav.sample_frames * wav.channels * sizeof(e3d::i16);
        void* memory = linearAlloc(bytes);
        if (memory == nullptr) return {};

        std::memcpy(memory, wav.samples, bytes);
        DSP_FlushDataCache(memory, bytes);

        ClipSlot clip;
        clip.data = memory;
        clip.sample_frames = wav.sample_frames;
        clip.channels = wav.channels;
        clip.sample_rate = wav.sample_rate;
        clip.valid = true;
        return clips_.add(clip);
    }

    void destroy_clip(e3d::ClipHandle handle) override {
        ClipSlot* clip = clips_.get(handle);
        if (clip == nullptr) return;

        for (int channel = 0; channel < kChannelCount; ++channel) {
            if (voices_[channel].active && voices_[channel].clip == handle) {
                ndspChnWaveBufClear(channel);
                voices_[channel].active = false;
            }
        }

        if (clip->data != nullptr) linearFree(clip->data);
        clip->data = nullptr;
        clip->valid = false;
        clips_.remove(handle);
    }

    e3d::VoiceHandle play(e3d::ClipHandle clip_handle, const e3d::PlayParams& params) override {
        if (!initialized_) return {};
        ClipSlot* clip = clips_.get(clip_handle);
        if (clip == nullptr || !clip->valid || clip->data == nullptr) return {};

        const int channel = find_channel();
        if (channel < 0) return {};

        VoiceSlot& voice = voices_[channel];
        ++voice.generation;
        if (voice.generation == 0) voice.generation = 1;
        voice.clip = clip_handle;
        voice.active = true;

        ndspChnReset(channel);
        ndspChnSetInterp(channel, NDSP_INTERP_LINEAR);
        ndspChnSetRate(channel, static_cast<float>(clip->sample_rate) * params.rate);
        ndspChnSetFormat(channel,
                         clip->channels == 1 ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16);

        const float pan = params.pan < -1.0f ? -1.0f : (params.pan > 1.0f ? 1.0f : params.pan);
        const float volume = params.volume < 0.0f ? 0.0f : params.volume;
        const float left = volume * (pan <= 0.0f ? 1.0f : 1.0f - pan);
        const float right = volume * (pan >= 0.0f ? 1.0f : 1.0f + pan);
        set_mix(channel, left, right);

        ndspWaveBuf& buffer = wave_buffers_[channel];
        std::memset(&buffer, 0, sizeof(buffer));
        buffer.data_vaddr = clip->data;
        buffer.nsamples = clip->sample_frames;
        buffer.looping = params.loop;
        ndspChnWaveBufAdd(channel, &buffer);

        return e3d::VoiceHandle{static_cast<e3d::u32>(channel), voice.generation};
    }

    void stop(e3d::VoiceHandle handle) override {
        if (handle.index >= static_cast<e3d::u32>(kChannelCount)) return;
        VoiceSlot& voice = voices_[handle.index];
        if (!voice.active || voice.generation != handle.generation) return;
        ndspChnWaveBufClear(static_cast<int>(handle.index));
        voice.active = false;
    }

    bool is_playing(e3d::VoiceHandle handle) const override {
        if (handle.index >= static_cast<e3d::u32>(kChannelCount)) return false;
        const VoiceSlot& voice = voices_[handle.index];
        if (!voice.active || voice.generation != handle.generation) return false;
        return ndspChnIsPlaying(static_cast<int>(handle.index));
    }

    void set_master_volume(float volume) override { ndspSetMasterVol(volume); }

    void update() override {
        for (int channel = 0; channel < kChannelCount; ++channel) {
            if (voices_[channel].active && !ndspChnIsPlaying(channel)) {
                voices_[channel].active = false;
            }
        }
    }

private:
    static void set_mix(int channel, float left, float right) {
        float mix[12] = {};
        mix[0] = left;
        mix[1] = right;
        ndspChnSetMix(channel, mix);
    }

    int find_channel() {
        for (int channel = 0; channel < kChannelCount; ++channel) {
            if (!voices_[channel].active || !ndspChnIsPlaying(channel)) {
                return channel;
            }
        }
        return -1;
    }

    bool initialized_ = false;
    e3d::HandlePool<e3d::ClipTag, ClipSlot> clips_;
    VoiceSlot voices_[kChannelCount];
    ndspWaveBuf wave_buffers_[kChannelCount];
};

} // namespace

namespace e3d {

std::unique_ptr<AudioDevice> create_audio_device_3ds() {
    return std::make_unique<AudioDevice3DS>();
}

u32 audio_device_3ds_last_result() { return static_cast<u32>(g_last_init_result); }

} // namespace e3d
