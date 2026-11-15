#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace e3d::desktop {

/// Decoded image, top-down RGBA8. The GL uploader flips rows on upload.
struct DecodedImage final {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

/// Decodes a tex3ds `.t3x` file (RGBA8/RGB8/RGBA5551/RGB565/RGBA4444/LA8/
/// HILO8/L8/A8/LA44/L4/A4, uncompressed or RLE/LZ10/LZ11). ETC1 is not
/// supported by the desktop backend.
bool decode_t3x(const void* data, std::size_t size, DecodedImage& out);

} // namespace e3d::desktop
