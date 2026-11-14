#include "e3d/audio/wav.hpp"

#include <cstring>

namespace {

e3d::u16 read_u16(const unsigned char* p) {
    return static_cast<e3d::u16>(p[0] | (p[1] << 8));
}

e3d::u32 read_u32(const unsigned char* p) {
    return static_cast<e3d::u32>(p[0]) | (static_cast<e3d::u32>(p[1]) << 8) |
           (static_cast<e3d::u32>(p[2]) << 16) | (static_cast<e3d::u32>(p[3]) << 24);
}

bool tag_equals(const unsigned char* p, const char* tag) {
    return std::memcmp(p, tag, 4) == 0;
}

} // namespace

namespace e3d::wav {

bool parse_wav(const void* data, std::size_t size, WavInfo& out) {
    out = {};

    if (data == nullptr || size < 12) return false;
    const auto* bytes = static_cast<const unsigned char*>(data);

    if (!tag_equals(bytes, "RIFF") || !tag_equals(bytes + 8, "WAVE")) return false;

    const unsigned char* cursor = bytes + 12;
    const unsigned char* end = bytes + size;

    u16 format = 0;
    u16 channels = 0;
    u32 sample_rate = 0;
    u16 bits = 0;
    const unsigned char* sample_data = nullptr;
    u32 sample_bytes = 0;

    while (cursor + 8 <= end) {
        const unsigned char* id = cursor;
        const u32 chunk_size = read_u32(cursor + 4);
        const unsigned char* payload = cursor + 8;

        if (payload + chunk_size > end) return false;

        if (tag_equals(id, "fmt ")) {
            if (chunk_size < 16) return false;
            format = read_u16(payload);
            channels = read_u16(payload + 2);
            sample_rate = read_u32(payload + 4);
            bits = read_u16(payload + 14);
        } else if (tag_equals(id, "data")) {
            sample_data = payload;
            sample_bytes = chunk_size;
        }

        cursor = payload + chunk_size + (chunk_size & 1u);
    }

    if (format != 1 || bits != 16 || channels == 0 || sample_rate == 0) return false;
    if (sample_data == nullptr || sample_bytes < channels * 2u) return false;

    out.samples = reinterpret_cast<const i16*>(sample_data);
    out.channels = channels;
    out.sample_rate = sample_rate;
    out.sample_frames = sample_bytes / (channels * 2u);
    return true;
}

} // namespace e3d::wav
