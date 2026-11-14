#include "e3d/core/guid.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

int hex_digit(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

e3d::u64 g_state = 0;
e3d::u64 g_counter = 0;

} // namespace

namespace e3d {

std::string guid_to_string(const Guid& guid) {
    const u32 a = static_cast<u32>(guid.hi >> 32);
    const u32 b = static_cast<u32>((guid.hi >> 16) & 0xFFFFu);
    const u32 c = static_cast<u32>(guid.hi & 0xFFFFu);
    const u32 d = static_cast<u32>((guid.lo >> 48) & 0xFFFFu);
    const u64 e = guid.lo & 0xFFFFFFFFFFFFull;

    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%08x-%04x-%04x-%04x-%012llx", a, b, c, d,
                  static_cast<unsigned long long>(e));
    return std::string(buffer);
}

bool guid_from_string(std::string_view text, Guid& out) {
    char digits[32];
    int count = 0;

    for (char c : text) {
        if (c == '-') continue;
        if (hex_digit(c) < 0) return false;
        if (count >= 32) return false;
        digits[count++] = c;
    }
    if (count != 32) return false;

    u8 bytes[16];
    for (int i = 0; i < 16; ++i) {
        bytes[i] = static_cast<u8>((hex_digit(digits[i * 2]) << 4) | hex_digit(digits[i * 2 + 1]));
    }

    u64 hi = 0;
    u64 lo = 0;
    for (int i = 0; i < 8; ++i) hi = (hi << 8) | bytes[i];
    for (int i = 8; i < 16; ++i) lo = (lo << 8) | bytes[i];

    out.hi = hi;
    out.lo = lo;
    return out.valid();
}

Guid generate_guid() {
    if (g_state == 0) {
        g_state = static_cast<u64>(std::time(nullptr)) * 0x9E3779B97F4A7C15ull;
        g_state ^= static_cast<u64>(reinterpret_cast<std::uintptr_t>(&g_state));
        if (g_state == 0) g_state = 0x123456789ABCDEFull;
    }

    auto next = []() noexcept {
        g_state ^= g_state >> 12;
        g_state ^= g_state << 25;
        g_state ^= g_state >> 27;
        return g_state * 2685821657736338717ull;
    };

    Guid guid;
    do {
        guid.hi = next();
        guid.lo = next() ^ (++g_counter);
    } while (!guid.valid());

    // RFC 4122 version 4 + variant bits, so guids look conventional.
    guid.hi = (guid.hi & 0xFFFFFFFFFFFF0FFFull) | 0x0000000000004000ull;
    guid.lo = (guid.lo & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull;
    return guid;
}

} // namespace e3d
