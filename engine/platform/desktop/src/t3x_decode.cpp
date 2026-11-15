#include "t3x_decode.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace e3d::desktop {

namespace {

std::uint16_t read_u16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
}

std::uint32_t read_u24(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16);
}

std::uint32_t read_u32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

int bytes_per_pixel(int format) {
    switch (format) {
        case 0: return 4;  // RGBA8
        case 1: return 3;  // RGB8
        case 2:
        case 3:
        case 4:
        case 5:
        case 6: return 2;
        case 7:
        case 8:
        case 9:
        case 10:
        case 11: return 1;
        default: return 0;  // ETC1 / ETC1A4
    }
}

/// Nintendo RLE (type 0x30): 0x80|len-3 repeats the next byte, otherwise
/// len = byte+1 literal bytes follow.
bool rle_decode(const std::uint8_t* src, std::size_t src_size, std::uint8_t* dst,
                std::size_t dst_size) {
    std::size_t in = 0;
    std::size_t out = 0;
    while (out < dst_size) {
        if (in >= src_size) return false;
        const std::uint8_t header = src[in++];
        if ((header & 0x80) != 0) {
            const std::size_t count = (header & 0x7F) + 3;
            if (in >= src_size) return false;
            const std::uint8_t value = src[in++];
            const std::size_t write = std::min(count, dst_size - out);
            std::memset(dst + out, value, write);
            out += write;
        } else {
            const std::size_t count = (header & 0x7F) + 1;
            if (in + count > src_size) return false;
            const std::size_t write = std::min(count, dst_size - out);
            std::memcpy(dst + out, src + in, write);
            in += write;
            out += write;
        }
    }
    return true;
}

bool lz10_decode(const std::uint8_t* src, std::size_t src_size, std::uint8_t* dst,
                 std::size_t dst_size) {
    std::size_t in = 0;
    std::size_t out = 0;
    while (out < dst_size) {
        if (in >= src_size) return false;
        std::uint8_t flags = src[in++];
        for (int bit = 0; bit < 8 && out < dst_size; ++bit, flags <<= 1) {
            if ((flags & 0x80) != 0) {
                if (in + 2 > src_size) return false;
                const std::uint8_t b1 = src[in++];
                const std::uint8_t b2 = src[in++];
                std::size_t length = (b1 >> 4) + 3;
                const std::size_t displacement = ((b1 & 0x0F) << 8) | b2;
                if (displacement + 1 > out) return false;
                const std::size_t write = std::min(length, dst_size - out);
                const std::uint8_t* back = dst + out - displacement - 1;
                for (std::size_t i = 0; i < write; ++i) dst[out + i] = back[i];
                out += write;
            } else {
                if (in >= src_size) return false;
                dst[out++] = src[in++];
            }
        }
    }
    return true;
}

bool lz11_decode(const std::uint8_t* src, std::size_t src_size, std::uint8_t* dst,
                 std::size_t dst_size) {
    std::size_t in = 0;
    std::size_t out = 0;
    while (out < dst_size) {
        if (in >= src_size) return false;
        std::uint8_t flags = src[in++];
        for (int bit = 0; bit < 8 && out < dst_size; ++bit, flags <<= 1) {
            if ((flags & 0x80) != 0) {
                if (in >= src_size) return false;
                const std::uint8_t head = src[in];
                const std::uint8_t mode = head >> 4;
                const std::size_t need = mode == 0 ? 3u : (mode == 1 ? 4u : 2u);
                if (in + need > src_size) return false;

                std::size_t length = 0;
                if (mode == 0) {
                    length = (static_cast<std::size_t>(head) << 4) | (src[in + 1] >> 4);
                    length += 0x11;
                    in += 1;
                } else if (mode == 1) {
                    length = ((static_cast<std::size_t>(head) & 0x0F) << 12) |
                             (static_cast<std::size_t>(src[in + 1]) << 4) |
                             (src[in + 2] >> 4);
                    length += 0x111;
                    in += 2;
                } else {
                    length = mode + 1;
                }

                const std::size_t displacement =
                    ((static_cast<std::size_t>(src[in]) & 0x0F) << 8) | src[in + 1];
                in += 2;
                if (displacement + 1 > out) return false;

                const std::size_t write = std::min(length, dst_size - out);
                const std::uint8_t* back = dst + out - displacement - 1;
                for (std::size_t i = 0; i < write; ++i) dst[out + i] = back[i];
                out += write;
            } else {
                if (in >= src_size) return false;
                dst[out++] = src[in++];
            }
        }
    }
    return true;
}

/// tex3ds swizzles each 8x8 tile in place. The forward permutation is replayed
/// on an index table so we can invert it.
const std::array<std::uint8_t, 64>& swizzle_order() {
    static const std::array<std::uint8_t, 64> order = [] {
        static constexpr unsigned char kFoursome[][4] = {
            {2, 8, 16, 4},   {3, 9, 17, 5},   {6, 10, 24, 20}, {7, 11, 25, 21},
            {14, 26, 28, 22}, {15, 27, 29, 23}, {34, 40, 48, 36}, {35, 41, 49, 37},
            {38, 42, 56, 52}, {39, 43, 57, 53}, {46, 58, 60, 54}, {47, 59, 61, 55},
        };

        std::array<std::uint8_t, 64> p{};
        for (std::uint8_t i = 0; i < 64; ++i) p[i] = i;

        for (const auto& entry : kFoursome) {
            const std::uint8_t tmp = p[entry[0]];
            p[entry[0]] = p[entry[1]];
            p[entry[1]] = p[entry[2]];
            p[entry[2]] = p[entry[3]];
            p[entry[3]] = tmp;
        }
        std::swap(p[12], p[18]);
        std::swap(p[13], p[19]);
        std::swap(p[44], p[50]);
        std::swap(p[45], p[51]);
        return p;
    }();
    return order;
}

void deswizzle(const std::uint8_t* src, std::uint8_t* dst, int width, int height,
               int pixel_size) {
    // The file stores 8x8 tiles packed back to back (row of tiles first), and
    // each tile block is the tex3ds permutation of the tile's row-major pixels.
    const auto& order = swizzle_order();
    const int tiles_per_row = width / 8;
    for (int ty = 0; ty < height; ty += 8) {
        for (int tx = 0; tx < width; tx += 8) {
            const std::size_t block =
                (static_cast<std::size_t>(ty / 8) * tiles_per_row + tx / 8) * 64;
            for (int pos = 0; pos < 64; ++pos) {
                const int original = order[pos];
                const std::size_t src_pixel = block + pos;
                const std::size_t dst_pixel =
                    static_cast<std::size_t>(ty + original / 8) * width + (tx + original % 8);
                std::memcpy(dst + dst_pixel * pixel_size, src + src_pixel * pixel_size,
                            static_cast<std::size_t>(pixel_size));
            }
        }
    }
}

std::uint8_t expand5(std::uint32_t value) {
    return static_cast<std::uint8_t>((value * 255 + 15) / 31);
}

std::uint8_t expand6(std::uint32_t value) {
    return static_cast<std::uint8_t>((value * 255 + 31) / 63);
}

std::uint8_t expand4(std::uint32_t value) {
    return static_cast<std::uint8_t>(value * 17);
}

} // namespace

bool decode_t3x(const void* data, std::size_t size, DecodedImage& out) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (bytes == nullptr || size < 17) return false;

    const std::uint16_t subimages = read_u16(bytes);
    const std::uint8_t params = bytes[2];
    const int format = bytes[3];
    const int mipmaps = bytes[4];
    const int width = 8 << (params & 0x07);
    const int height = 8 << ((params >> 3) & 0x07);
    const int bpp = bytes_per_pixel(format);
    if (subimages == 0 || bpp == 0 || mipmaps != 0 || width <= 0 || height <= 0) return false;

    const std::size_t header = 5 + static_cast<std::size_t>(subimages) * 12;
    if (header + 4 > size) return false;

    std::size_t pos = header;
    std::uint8_t compression = bytes[pos++];
    std::size_t raw_size = 0;
    if ((compression & 0x80) != 0) {
        compression = static_cast<std::uint8_t>(compression & 0x7F);
        if (pos + 7 > size) return false;
        raw_size = read_u32(bytes + pos);
        pos += 7;  // 4 size bytes + 3 reserved
    } else {
        if (pos + 3 > size) return false;
        raw_size = read_u24(bytes + pos);
        pos += 3;
    }

    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * bpp;
    if (raw_size < expected || pos > size) return false;

    std::vector<std::uint8_t> raw(raw_size);
    bool decoded = false;
    switch (compression) {
        case 0x00: {
            const std::size_t available = std::min(raw_size, size - pos);
            std::memcpy(raw.data(), bytes + pos, available);
            decoded = available == raw_size;
            break;
        }
        case 0x10:
            decoded = lz10_decode(bytes + pos, size - pos, raw.data(), raw_size);
            break;
        case 0x11:
            decoded = lz11_decode(bytes + pos, size - pos, raw.data(), raw_size);
            break;
        case 0x30:
            decoded = rle_decode(bytes + pos, size - pos, raw.data(), raw_size);
            break;
        default:
            return false;  // Huffman is not implemented.
    }
    if (!decoded) return false;

    std::vector<std::uint8_t> linear(expected);
    deswizzle(raw.data(), linear.data(), width, height, bpp);

    out.width = width;
    out.height = height;
    out.rgba.assign(static_cast<std::size_t>(width) * height * 4u, 0);

    for (int y = 0; y < height; ++y) {
        const int gl_y = height - 1 - y;  // flip for GL's bottom-left origin
        for (int x = 0; x < width; ++x) {
            const std::size_t src_index = (static_cast<std::size_t>(y) * width + x) * bpp;
            std::uint8_t* dst = out.rgba.data() +
                                (static_cast<std::size_t>(gl_y) * width + x) * 4u;
            const std::uint8_t* src = linear.data() + src_index;

            switch (format) {
                case 0:  // ABGR in memory
                    dst[0] = src[3];
                    dst[1] = src[2];
                    dst[2] = src[1];
                    dst[3] = src[0];
                    break;
                case 1:  // BGR
                    dst[0] = src[2];
                    dst[1] = src[1];
                    dst[2] = src[0];
                    dst[3] = 255;
                    break;
                case 2: {  // RGBA5551
                    const std::uint16_t v = read_u16(src);
                    dst[0] = expand5((v >> 11) & 0x1F);
                    dst[1] = expand5((v >> 6) & 0x1F);
                    dst[2] = expand5((v >> 1) & 0x1F);
                    dst[3] = (v & 1) != 0 ? 255 : 0;
                    break;
                }
                case 3: {  // RGB565
                    const std::uint16_t v = read_u16(src);
                    dst[0] = expand5((v >> 11) & 0x1F);
                    dst[1] = expand6((v >> 5) & 0x3F);
                    dst[2] = expand5(v & 0x1F);
                    dst[3] = 255;
                    break;
                }
                case 4: {  // RGBA4444
                    const std::uint16_t v = read_u16(src);
                    dst[0] = expand4((v >> 12) & 0x0F);
                    dst[1] = expand4((v >> 8) & 0x0F);
                    dst[2] = expand4((v >> 4) & 0x0F);
                    dst[3] = expand4(v & 0x0F);
                    break;
                }
                case 5:  // LA8 (A,L)
                    dst[0] = dst[1] = dst[2] = src[1];
                    dst[3] = src[0];
                    break;
                case 6:  // HILO8 (LO,HI)
                    dst[0] = src[1];
                    dst[1] = src[0];
                    dst[2] = 0;
                    dst[3] = 255;
                    break;
                case 7:  // L8
                    dst[0] = dst[1] = dst[2] = src[0];
                    dst[3] = 255;
                    break;
                case 8:  // A8
                    dst[0] = dst[1] = dst[2] = 0;
                    dst[3] = src[0];
                    break;
                case 9:  // LA44
                    dst[0] = dst[1] = dst[2] = expand4((src[0] >> 4) & 0x0F);
                    dst[3] = expand4(src[0] & 0x0F);
                    break;
                case 10:  // L4, first pixel low nibble
                case 11: {  // A4
                    const bool second = (x & 1) != 0;
                    const std::uint8_t nibble =
                        second ? static_cast<std::uint8_t>((src[0] >> 4) & 0x0F)
                               : static_cast<std::uint8_t>(src[0] & 0x0F);
                    if (format == 10) {
                        dst[0] = dst[1] = dst[2] = expand4(nibble);
                        dst[3] = 255;
                    } else {
                        dst[0] = dst[1] = dst[2] = 0;
                        dst[3] = expand4(nibble);
                    }
                    break;
                }
                default:
                    return false;
            }
        }
    }

    return true;
}

} // namespace e3d::desktop
